#include "gi_denoiser.h"
#include "../renderer.h"

GIDenoiser::GIDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("recurrent_blur/temporal_accumulation.hlsl", "main", "cs_6_6", {});
    m_pTemporalAccumulationPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/temporal accumulation PSO");
}

void GIDenoiser::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pTemporalAccumulation0 == nullptr ||
        m_pTemporalAccumulation0->GetTexture()->GetDesc().width != width ||
        m_pTemporalAccumulation0->GetTexture()->GetDesc().height != height)
    {
        m_pTemporalAccumulation0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation 0"));
        m_pTemporalAccumulation1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation 1"));

        m_bHistoryInvalid = true;
    }

    m_historyRadiance = pRenderGraph->Import(m_pTemporalAccumulation1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiser::Render(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "GIDenoiser");

    eastl::swap(m_pTemporalAccumulation0, m_pTemporalAccumulation1);

    struct PreBlurData
    {
        RGHandle input;
        RGHandle output;
    };

    auto pre_blur = pRenderGraph->AddPass<PreBlurData>("GI Denoiser - pre blur", RenderPassType::Compute,
        [&](PreBlurData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);


        },
        [=](const PreBlurData& data, IGfxCommandList* pCommandList)
        {
        });

    struct TemporalAccumulationData
    {
        RGHandle input;
        RGHandle history;
        RGHandle output;
    };

    auto temporal_accumulation = pRenderGraph->AddPass<TemporalAccumulationData>("GI Denoiser - temporal accumulation", RenderPassType::Compute,
        [&](TemporalAccumulationData& data, RGBuilder& builder)
        {
            RGHandle history = m_historyRadiance;
            RGHandle output = builder.Import(m_pTemporalAccumulation1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);

            data.input = builder.Read(input);
            data.history = builder.Read(history);
            data.output = builder.Write(output);
        },
        [=](const TemporalAccumulationData& data, IGfxCommandList* pCommandList)
        {
            pCommandList->SetPipelineState(m_pTemporalAccumulationPSO);

            uint32_t constants[3] = {
                pRenderGraph->GetTexture(input)->GetSRV()->GetHeapIndex(),
                m_pTemporalAccumulation0->GetSRV()->GetHeapIndex(),
                m_pTemporalAccumulation1->GetUAV()->GetHeapIndex(),
            };
            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });

    struct BlurData
    {
        RGHandle input;
        RGHandle output;
    };

    auto blur = pRenderGraph->AddPass<BlurData>("GI Denoiser - blur", RenderPassType::Compute,
        [&](BlurData& data, RGBuilder& builder)
        {
            data.input = builder.Read(temporal_accumulation->output);


        },
        [=](const BlurData& data, IGfxCommandList* pCommandList)
        {
        });

    return temporal_accumulation->output;
}
