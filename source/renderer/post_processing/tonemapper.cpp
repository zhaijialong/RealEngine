#include "tonemapper.h"
#include "../renderer.h"
#include "utils/gui_util.h"

Tonemapper::Tonemapper(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle Tonemapper::Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, RenderGraphHandle exposure, RenderGraphHandle bloom, float bloom_intensity, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "ToneMapping",
        [&]()
        {

        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle exposure;
        RenderGraphHandle bloom;

        RenderGraphHandle outLdrRT;
    };

    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.hdrRT = builder.Read(inputHandle, GfxResourceState::ShaderResourceNonPS);
            data.exposure = builder.Read(exposure, GfxResourceState::ShaderResourceNonPS);

            if (bloom.IsValid())
            {
                data.bloom = builder.Read(bloom, GfxResourceState::ShaderResourceNonPS);
            }

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

            IGfxDescriptor* bloomSRV = nullptr;
            if (data.bloom.IsValid())
            {
                RenderGraphTexture* bloom = (RenderGraphTexture*)pRenderGraph->GetResource(data.bloom);
                bloomSRV = bloom->GetSRV();
            }

            Draw(pCommandList, hdrRT->GetSRV(), exposureRT->GetSRV(), ldrRT->GetUAV(), bloomSRV, bloom_intensity, width, height);
        });

    return tonemap_pass->outLdrRT;
}

void Tonemapper::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* exposure, IGfxDescriptor* pLdrUAV, IGfxDescriptor* bloom, float bloom_intensity, uint32_t width, uint32_t height)
{
    std::vector<std::string> defines;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = m_pRenderer->GetShader("tone_mapping.hlsl", "cs_main", "cs_6_6", defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(psoDesc, "ToneMapping PSO");

    pCommandList->SetPipelineState(pso);

    struct Constants
    {
        uint hdrTexture;
        uint ldrTexture;
        uint exposureTexture;
        uint bloomTexture;
        float bloomIntensity;
    };
    
    Constants constants = {
        pHdrSRV->GetHeapIndex(),
        pLdrUAV->GetHeapIndex(),
        exposure->GetHeapIndex(),
        bloom ? bloom->GetHeapIndex() : GFX_INVALID_RESOURCE,
        bloom_intensity
    };

    pCommandList->SetComputeConstants(0, &constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
