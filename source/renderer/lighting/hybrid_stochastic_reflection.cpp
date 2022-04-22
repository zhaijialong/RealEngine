#include "hybrid_stochastic_reflection.h"
#include "../renderer.h"
#include "../hierarchical_depth_buffer.h"
#include "utils/gui_util.h"

HybridStochasticReflection::HybridStochasticReflection(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pDenoiser = eastl::make_unique<ReflectionDenoiser>(pRenderer);

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/ssr.hlsl", "main", "cs_6_6", {});
    m_pSSRPSO = pRenderer->GetPipelineState(desc, "SSR PSO");
}

RenderGraphHandle HybridStochasticReflection::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Hybrid Stochastic Reflection",
        [&]()
        {
            ImGui::Checkbox("Enable##HSR", &m_bEnable);
            ImGui::Checkbox("Half Resolution##HSR", &m_bHalfResolution);
            ImGui::Checkbox("Enable Denoiser##HSR", &m_bEnableDenoiser);
        });

    if (!m_bEnable)
    {
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "HybridStochasticReflection");

    HZB* pHZB = m_pRenderer->GetHZB();

    struct SSRData
    {
        RenderGraphHandle normal;
        RenderGraphHandle depth;
        RenderGraphHandle sceneHZB;
        RenderGraphHandle velocity;
        RenderGraphHandle prevSceneColor;
        RenderGraphHandle output;
    };

    auto ssr_pass = pRenderGraph->AddPass<SSRData>("SSR", RenderPassType::Compute,
        [&](SSRData& data, RenderGraphBuilder& builder)
        {
            data.normal = builder.Read(normal);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.prevSceneColor = builder.Read(m_pRenderer->GetPrevSceneColorHandle());

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.sceneHZB = builder.Read(pHZB->GetSceneHZBMip(i), i);
            }

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Create<RenderGraphTexture>(desc, "SSR output");
            data.output = builder.Write(data.output);
        },
        [=](const SSRData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* velocity = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            SSR(pCommandList, normal->GetSRV(), depth->GetSRV(), velocity->GetSRV(), output->GetUAV(), width, height);
        });
    
    return ssr_pass->output;
}

void HybridStochasticReflection::SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pSSRPSO);

    uint constants[5] = {
        normal->GetHeapIndex(),
        depth->GetHeapIndex(),
        velocity->GetHeapIndex(),
        m_pRenderer->GetPrevSceneColorTexture()->GetSRV()->GetHeapIndex(),
        outputUAV->GetHeapIndex()
    };

    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
