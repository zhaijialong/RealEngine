#include "restir_gi.h"
#include "../renderer.h"
#include "utils/gui_util.h"

ReSTIRGI::ReSTIRGI(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("restir_gi/initial_sampling.hlsl", "main", "cs_6_6", {});
    m_pInitialSamplingPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/initial sampling PSO");
}

RenderGraphHandle ReSTIRGI::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, uint32_t width, uint32_t height)
{
    GUI("Lighting", "ReSTIR GI",
        [&]()
        {
            ImGui::Checkbox("Enable##ReSTIR GI", &m_bEnable);
            ImGui::Checkbox("Enable ReSTIR##ReSTIR GI", &m_bEnableReSTIR);
            ImGui::Checkbox("Enable Denoiser##ReSTIR GI", &m_bEnableDenoiser);
        });

    if (!m_bEnable)
    {
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "ReSTIR GI");

    struct RaytracePassData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle outputIrradiance;
        RenderGraphHandle outputHitNormal;
        RenderGraphHandle outputRay; //xyz : rayDir * hitT, w : pdf
    };

    auto raytrace_pass = pRenderGraph->AddPass<RaytracePassData>("ReSTIR GI - initial sampling", RenderPassType::Compute,
        [&](RaytracePassData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.outputIrradiance = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate irradiance"));

            desc.format = GfxFormat::RG16UNORM;
            data.outputHitNormal = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate hitNormal"));

            desc.format = GfxFormat::RGBA16F;
            data.outputRay = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate ray"));
        },
        [=](const RaytracePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* outputIrradiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputIrradiance);
            RenderGraphTexture* outputHitNormal = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputHitNormal);
            RenderGraphTexture* outputRay = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRay);
            InitialSampling(pCommandList, depth->GetSRV(), normal->GetSRV(), outputIrradiance->GetUAV(), outputHitNormal->GetUAV(), outputRay->GetUAV(), width, height);
        });

    if (!m_bEnableReSTIR)
    {
        return raytrace_pass->outputIrradiance;
    }

    InitTemporalBuffers(width, height);

    return RenderGraphHandle();
}

void ReSTIRGI::InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal,
    IGfxDescriptor* outputIrradiance, IGfxDescriptor* outputHitNormal, IGfxDescriptor* outputRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pInitialSamplingPSO);

    uint32_t constants[5] = { 
        depth->GetHeapIndex(), 
        normal->GetHeapIndex(),
        outputIrradiance->GetHeapIndex(),
        outputHitNormal->GetHeapIndex(),
        outputRay->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::InitTemporalBuffers(uint32_t width, uint32_t height)
{
}
