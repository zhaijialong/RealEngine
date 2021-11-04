#include "post_processor.h"

PostProcessor::PostProcessor(Renderer* pRenderer)
{
    m_pTAA.reset(new TAA(pRenderer));
    m_pToneMapper.reset(new Tonemapper(pRenderer, DISPLAYMODE_SDR, ColorSpace_REC709));
    m_pFXAA.reset(new FXAA(pRenderer));
    m_pCAS.reset(new CAS(pRenderer));
}

RenderGraphHandle PostProcessor::Process(RenderGraph* pRenderGraph, const PostProcessInput& input, uint32_t width, uint32_t height)
{
    auto taa_velocity_pass = pRenderGraph->AddPass<TAAVelocityPassData>("TAA generate velocity",
        [&](TAAVelocityPassData& data, RenderGraphBuilder& builder)
        {
            data.inputDepthRT = builder.Read(input.sceneDepthRT, GfxResourceState::ShaderResourceNonPS);
            data.inputVelocityRT = builder.Read(input.velocityRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputMotionVectorRT = builder.Create<RenderGraphTexture>(desc, "TAA motion vector");
            data.outputMotionVectorRT = builder.Write(data.outputMotionVectorRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputDepthRT);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputVelocityRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputMotionVectorRT);

            m_pTAA->GenerateMotionVector(pCommandList, depthRT->GetSRV(), velocityRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    auto taa_pass = pRenderGraph->AddPass<TAAPassData>("TAA",
        [&](TAAPassData& data, RenderGraphBuilder& builder)
        {
            data.inputRT = builder.Read(input.sceneColorRT, GfxResourceState::ShaderResourceNonPS);
            data.velocityRT = builder.Read(taa_velocity_pass->outputMotionVectorRT, GfxResourceState::ShaderResourceNonPS);
            data.linearDepthRT = builder.Read(input.linearDepthRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Output");
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocityRT);
            RenderGraphTexture* linearDepthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.linearDepthRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRT);

            m_pTAA->Draw(pCommandList, inputRT->GetSRV(), velocityRT->GetSRV(), linearDepthRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    auto taa_copy_pass = pRenderGraph->AddPass<TAACopyPassData>("TAA copy history",
        [&](TAACopyPassData& data, RenderGraphBuilder& builder)
        {
            builder.MakeTarget();

            data.inputRT = builder.Read(taa_pass->outputRT, GfxResourceState::CopySrc);
        },
        [=](const TAACopyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);

            m_pTAA->CopyHistory(pCommandList, inputRT->GetTexture());
        });

    auto taa_apply_pass = pRenderGraph->AddPass<TAAApplyPassData>("TAA apply",
        [&](TAAApplyPassData& data, RenderGraphBuilder& builder)
        {
            data.inputRT = builder.Read(taa_pass->outputRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Final");
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAApplyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRT);

            m_pTAA->Apply(pCommandList, inputRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.inHdrRT = builder.Read(taa_apply_pass->outputRT, GfxResourceState::ShaderResourceNonPS);

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

    auto cas_pass = pRenderGraph->AddPass<CASPassData>("CAS",
        [&](CASPassData& data, RenderGraphBuilder& builder)
        {
            data.inRT = builder.Read(tonemap_pass->outLdrRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outRT = builder.Create<RenderGraphTexture>(desc, "CAS Output");
            data.outRT = builder.Write(data.outRT, GfxResourceState::UnorderedAccess);
        },
        [=](const CASPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inRT);
            RenderGraphTexture* outRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outRT);

            m_pCAS->Draw(pCommandList, inRT->GetSRV(), outRT->GetUAV(), width, height);
        });

    return cas_pass->outRT;
}
