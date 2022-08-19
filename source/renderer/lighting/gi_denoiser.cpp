#include "gi_denoiser.h"
#include "../renderer.h"

GIDenoiser::GIDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
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
        m_pTemporalAccumulationSH.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation history"));

        //m_pHistoryRadiance0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/history radiance 0"));
        m_pHistoryRadiance1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/history radiance 1"));

        m_bHistoryInvalid = true;
    }

    m_historyRadiance = pRenderGraph->Import(m_pHistoryRadiance1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiser::Render(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "GIDenoiser");

    /*
    struct PreBlurData
    {
        RGHandle inputSH;
        RGHandle outputSH;
    };

    auto pre_blur = pRenderGraph->AddPass<PreBlurData>("GI Denoiser - pre blur", RenderPassType::Compute,
        [&](PreBlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(radianceSH);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/pre blur output"));
        },
        [=](const PreBlurData& data, IGfxCommandList* pCommandList)
        {
        });
    */

    struct TemporalAccumulationData
    {
        RGHandle inputSH;
        RGHandle normal;
        RGHandle historySH;
        RGHandle outputSH;
    };

    auto temporal_accumulation = pRenderGraph->AddPass<TemporalAccumulationData>("GI Denoiser - temporal accumulation", RenderPassType::Compute,
        [&](TemporalAccumulationData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(radianceSH);
            data.normal = builder.Read(normal);
            data.historySH = builder.Read(builder.Import(m_pTemporalAccumulationSH->GetTexture(), GfxResourceState::UnorderedAccess));

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/temporal accumulation output"));
        },
        [=](const TemporalAccumulationData& data, IGfxCommandList* pCommandList)
        {
            pCommandList->SetPipelineState(m_pTemporalAccumulationPSO);

            uint32_t constants[4] = {
                pRenderGraph->GetTexture(data.inputSH)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.normal)->GetSRV()->GetHeapIndex(),
                m_pTemporalAccumulationSH->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputSH)->GetUAV()->GetHeapIndex(),
            };
            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });

    struct BlurData
    {
        RGHandle inputSH;
        RGHandle normal;
        RGHandle outputSH;
        RGHandle outputRadiance;
    };

    auto blur = pRenderGraph->AddPass<BlurData>("GI Denoiser - blur", RenderPassType::Compute,
        [&](BlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(temporal_accumulation->outputSH);
            data.normal = builder.Read(normal);
            data.outputSH = builder.Write(temporal_accumulation->historySH);

            data.outputRadiance = builder.Write(m_historyRadiance);
        },
        [=](const BlurData& data, IGfxCommandList* pCommandList)
        {
            pCommandList->SetPipelineState(m_pBlurPSO);

            uint32_t constants[4] = {
                pRenderGraph->GetTexture(data.inputSH)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.normal)->GetSRV()->GetHeapIndex(),
                m_pTemporalAccumulationSH->GetUAV()->GetHeapIndex(),
                m_pHistoryRadiance1->GetUAV()->GetHeapIndex(),
            };
            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });

    return blur->outputRadiance;
}
