#include "tonemapper.h"
#include "../renderer.h"
#include "utils/gui_util.h"

Tonemapper::Tonemapper(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle Tonemapper::Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, RenderGraphHandle exposure, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "ToneMapping",
        [&]()
        {

        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle exposure;

        RenderGraphHandle outLdrRT;
    };

    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.hdrRT = builder.Read(inputHandle, GfxResourceState::ShaderResourceNonPS);
            data.exposure = builder.Read(exposure, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outLdrRT = builder.Create<RenderGraphTexture>(desc, "ToneMapping Output");
            data.outLdrRT = builder.Write(data.outLdrRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outLdrRT);
            RenderGraphTexture* exposureRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.exposure);

            Draw(pCommandList, hdrRT->GetSRV(), exposureRT->GetSRV(), ldrRT->GetUAV(), width, height);
        });

    return tonemap_pass->outLdrRT;
}

void Tonemapper::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* exposure, IGfxDescriptor* pLdrUAV, uint32_t width, uint32_t height)
{
    std::vector<std::string> defines;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = m_pRenderer->GetShader("tone_mapping.hlsl", "cs_main", "cs_6_6", defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(psoDesc, "ToneMapping PSO");

    pCommandList->SetPipelineState(pso);

    uint32_t resourceCB[3] = { 
        pHdrSRV->GetHeapIndex(), 
        pLdrUAV->GetHeapIndex(), 
        exposure->GetHeapIndex(),
    };
    pCommandList->SetComputeConstants(0, resourceCB, sizeof(resourceCB));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
