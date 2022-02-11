#include "tonemapper.h"
#include "../renderer.h"

Tonemapper::Tonemapper(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("tone_mapping.hlsl", "cs_main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "LPM PSO");
}

RenderGraphHandle Tonemapper::Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, uint32_t width, uint32_t height)
{
    struct TonemapPassData
    {
        RenderGraphHandle inHdrRT;
        RenderGraphHandle outLdrRT;
    };

    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.inHdrRT = builder.Read(inputHandle, GfxResourceState::ShaderResourceNonPS);

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
            RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inHdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outLdrRT);

            Draw(pCommandList, hdrRT->GetSRV(), ldrRT->GetUAV(), width, height);
        });

    return tonemap_pass->outLdrRT;
}

void Tonemapper::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* pLdrUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    uint32_t resourceCB[4] = { pHdrSRV->GetHeapIndex(), pLdrUAV->GetHeapIndex(), width, height };
    pCommandList->SetComputeConstants(0, resourceCB, sizeof(resourceCB));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
