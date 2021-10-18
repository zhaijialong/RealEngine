#include "post_processor.h"

PostProcessor::PostProcessor(Renderer* pRenderer)
{
    m_pFXAA.reset(new FXAA(pRenderer));
    m_pToneMapper.reset(new Tonemapper(pRenderer, DISPLAYMODE_SDR, ColorSpace_REC709));
}

RenderGraphHandle PostProcessor::Process(RenderGraph* pRenderGraph, const PostProcessInput& input, uint32_t width, uint32_t height)
{
    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.inHdrRT = builder.Read(input.sceneColorRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outLdrRT = builder.Create<RenderGraphTexture>(desc, "ToneMapping Output");
            data.outLdrRT = builder.Write(data.outLdrRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inHdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outLdrRT);

            m_pToneMapper->Draw(pCommandList, hdrRT->GetSRV(), ldrRT->GetUAV(), width, height);
        });


    auto fxaa_pass = pRenderGraph->AddPass<FXAAPassData>("FXAA",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            data.inRT = builder.Read(tonemap_pass->outLdrRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outRT = builder.Create<RenderGraphTexture>(desc, "FXAA Output");
            data.outRT = builder.Write(data.outRT, GfxResourceState::UnorderedAccess);
        },
        [=](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inRT);
            RenderGraphTexture* outRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outRT);

            m_pFXAA->Draw(pCommandList, inRT->GetSRV(), outRT->GetUAV(), width, height);
        });

    return fxaa_pass->outRT;
}
