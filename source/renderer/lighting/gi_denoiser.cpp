#include "gi_denoiser.h"
#include "../renderer.h"

GIDenoiser::GIDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("recurrent_blur/pre_blur.hlsl", "main", "cs_6_6", {});
    m_pPreBlurPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/pre blur PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/temporal_accumulation.hlsl", "main", "cs_6_6", {});
    m_pTemporalAccumulationPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/temporal accumulation PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/blur.hlsl", "main", "cs_6_6", {});
    m_pBlurPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/blur PSO");
}

void GIDenoiser::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pTemporalAccumulationSH == nullptr ||
        m_pTemporalAccumulationSH->GetTexture()->GetDesc().width != width ||
        m_pTemporalAccumulationSH->GetTexture()->GetDesc().height != height)
    {
        m_pTemporalAccumulationSH.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation historySH"));
        m_pTemporalAccumulationCount0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation count 0"));
        m_pTemporalAccumulationCount1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation count 1"));

        //m_pHistoryRadiance0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/history radiance 0"));
        m_pHistoryRadiance1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/history radiance 1"));

        m_bHistoryInvalid = true;
    }

    m_historyRadiance = pRenderGraph->Import(m_pHistoryRadiance1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiser::Render(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "GI Denoiser");

    struct PreBlurData
    {
        RGHandle inputSH;
        RGHandle depth;
        RGHandle normal;
        RGHandle outputSH;
    };

    auto pre_blur = pRenderGraph->AddPass<PreBlurData>("GI Denoiser - pre blur", RenderPassType::Compute,
        [&](PreBlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(radianceSH);
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/pre blur outputSH"));
        },
        [=](const PreBlurData& data, IGfxCommandList* pCommandList)
        {
            PreBlur(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.outputSH),
                width, height);
        });

    eastl::swap(m_pTemporalAccumulationCount0, m_pTemporalAccumulationCount1);

    struct TemporalAccumulationData
    {
        RGHandle inputSH;
        RGHandle historySH;
        RGHandle historyAccumulationCount;
        RGHandle depth;
        RGHandle normal;
        RGHandle velocity;
        RGHandle prevLinearDepth;
        RGHandle prevNormal;
        RGHandle outputSH;
        RGHandle outputAccumulationCount;
    };

    auto temporal_accumulation = pRenderGraph->AddPass<TemporalAccumulationData>("GI Denoiser - temporal accumulation", RenderPassType::Compute,
        [&](TemporalAccumulationData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(pre_blur->outputSH);
            data.historySH = builder.Read(builder.Import(m_pTemporalAccumulationSH->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyAccumulationCount = builder.Read(builder.Import(m_pTemporalAccumulationCount0->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.velocity = builder.Read(velocity);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevNormal = builder.Read(m_pRenderer->GetPrevNormalHandle());

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/temporal accumulation outputSH"));

            data.outputAccumulationCount = builder.Write(builder.Import(m_pTemporalAccumulationCount1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
        },
        [=](const TemporalAccumulationData& data, IGfxCommandList* pCommandList)
        {
            TemporalAccumulation(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.outputSH),
                width, height);
        });

    struct BlurData
    {
        RGHandle inputSH;
        RGHandle accumulationCount;
        RGHandle depth;
        RGHandle normal;
        RGHandle outputSH;
        RGHandle outputRadiance;
    };

    auto blur = pRenderGraph->AddPass<BlurData>("GI Denoiser - blur", RenderPassType::Compute,
        [&](BlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(temporal_accumulation->outputSH);
            data.accumulationCount = builder.Read(temporal_accumulation->outputAccumulationCount);
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.outputSH = builder.Write(temporal_accumulation->historySH);

            data.outputRadiance = builder.Write(pRenderGraph->Import(m_pHistoryRadiance1->GetTexture(), GfxResourceState::ShaderResourceNonPS));
        },
        [=](const BlurData& data, IGfxCommandList* pCommandList)
        {
            Blur(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                width, height);
        });

    return blur->outputRadiance;
}

void GIDenoiser::PreBlur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* outputSH, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPreBlurPSO);

    struct CB
    {
        uint inputSHTexture;
        uint depthTexture;
        uint normalTexture;
        uint outputSHTexture;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = outputSH->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void GIDenoiser::TemporalAccumulation(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
    RGTexture* outputSH, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalAccumulationPSO);

    struct CB
    {
        uint inputSHTexture;
        uint historySHTexture;
        uint historyAccumulationCountTexture;
        uint depthTexture;
        uint normalTexture;
        uint velocityTexture;
        uint prevLinearDepthTexture;
        uint prevNormalTexture;
        uint outputSHTexture;
        uint outputAccumulationCountTexture;
        uint bHistoryInvalid;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.historySHTexture = m_pTemporalAccumulationSH->GetSRV()->GetHeapIndex();
    constants.historyAccumulationCountTexture = m_pTemporalAccumulationCount0->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.velocityTexture = velocity->GetSRV()->GetHeapIndex();
    constants.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    constants.prevNormalTexture = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = outputSH->GetUAV()->GetHeapIndex();
    constants.outputAccumulationCountTexture = m_pTemporalAccumulationCount1->GetUAV()->GetHeapIndex();
    constants.bHistoryInvalid = m_bHistoryInvalid;

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;
    }
}

void GIDenoiser::Blur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pBlurPSO);

    struct CB
    {
        uint inputSHTexture;
        uint accumulationCountTexture;
        uint depthTexture;
        uint normalTexture;
        uint outputSHTexture;
        uint outputRadianceTexture;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.accumulationCountTexture = m_pTemporalAccumulationCount1->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = m_pTemporalAccumulationSH->GetUAV()->GetHeapIndex();
    constants.outputRadianceTexture = m_pHistoryRadiance1->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
