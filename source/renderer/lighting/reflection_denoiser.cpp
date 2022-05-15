#include "reflection_denoiser.h"
#include "../renderer.h"

ReflectionDenoiser::ReflectionDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_reproject.hlsl", "main", "cs_6_6", {});
    m_pReprojectPSO = pRenderer->GetPipelineState(desc, "HSR denoiser reproject PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_prefilter.hlsl", "main", "cs_6_6", {});
    m_pPrefilterPSO = pRenderer->GetPipelineState(desc, "HSR denoiser prefilter PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_resolve_temporal.hlsl", "main", "cs_6_6", {});
    m_pResolveTemporalPSO = pRenderer->GetPipelineState(desc, "HSR denoiser resolve temporal PSO");
}

void ReflectionDenoiser::ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pRandianceHistory == nullptr ||
        m_pRandianceHistory->GetTexture()->GetDesc().width != width ||
        m_pRandianceHistory->GetTexture()->GetDesc().height != height)
    {
        m_bHistoryInvalid = true;
        m_pRandianceHistory.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pRandianceHistory"));
        m_pVarianceHistory.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pRandianceHistory"));

        m_pSampleCountInput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pSampleCount0"));
        m_pSampleCountOutput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pSampleCount1"));
    }

    eastl::swap(m_pSampleCountInput, m_pSampleCountOutput);

    m_randianceHistory = pRenderGraph->Import(m_pRandianceHistory->GetTexture(), GfxResourceState::UnorderedAccess);
    m_varianceHistory = pRenderGraph->Import(m_pVarianceHistory->GetTexture(), GfxResourceState::UnorderedAccess);
    m_sampleCountInput = pRenderGraph->Import(m_pSampleCountInput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
    m_sampleCountOutput = pRenderGraph->Import(m_pSampleCountOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RenderGraphHandle ReflectionDenoiser::Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "ReflectionDenoiser");

    struct ReprojectPassData
    {
        RenderGraphHandle indirectArgs;
        RenderGraphHandle tileListBuffer;

        RenderGraphHandle inputRadiance;
        RenderGraphHandle historyRadiance;
        RenderGraphHandle historyVariance;
        RenderGraphHandle historySampleCount;

        RenderGraphHandle outputRadiance;
        RenderGraphHandle outputVariance;
        RenderGraphHandle outputAvgRadiance;
        RenderGraphHandle outputSampleCount;
    };

    auto reproject_pass = pRenderGraph->AddPass<ReprojectPassData>("ReflectionDenoiser - Reproject", RenderPassType::Compute,
        [&](ReprojectPassData& data, RenderGraphBuilder& builder)
        {
            data.indirectArgs = builder.ReadIndirectArg(indirectArgs);
            data.tileListBuffer = builder.Read(tileListBuffer);
            
            data.inputRadiance = builder.Read(input);
            data.historyRadiance = builder.Read(m_randianceHistory);
            data.historyVariance = builder.Read(m_varianceHistory);
            data.historySampleCount = builder.Read(m_sampleCountInput);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputRadiance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser reprojected radiance");
            data.outputRadiance = builder.Write(data.outputRadiance);

            desc.format = GfxFormat::R16F;
            data.outputVariance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser reprojected variance");
            data.outputVariance = builder.Write(data.outputVariance);
             
            desc.width = (width + 7) / 8;
            desc.height = (height + 7) / 8;
            desc.format = GfxFormat::RGBA16F;
            data.outputAvgRadiance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser average variance");
            data.outputAvgRadiance = builder.Write(data.outputAvgRadiance);

            data.outputSampleCount = builder.Write(m_sampleCountOutput);
        },
        [=](const ReprojectPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* indirectArgs = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgs);
            RenderGraphBuffer* tileListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileListBuffer);

            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRadiance);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRadiance);

            pCommandList->SetPipelineState(m_pReprojectPSO);

            uint32_t constants[3] = { tileListBuffer->GetSRV()->GetHeapIndex(), input->GetSRV()->GetHeapIndex(), output->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);

            if (m_bHistoryInvalid)
            {
                m_bHistoryInvalid = false;
            }
        });

    struct PrefilterPassData
    {
        RenderGraphHandle inputRadiance;
        RenderGraphHandle inputVariance;
        RenderGraphHandle avgRadiance;

        RenderGraphHandle outputRadiance;
        RenderGraphHandle outputVariance;
    };

    auto prefilter_pass = pRenderGraph->AddPass<PrefilterPassData>("ReflectionDenoiser - Prefilter", RenderPassType::Compute,
        [&](PrefilterPassData& data, RenderGraphBuilder& builder)
        {
            data.inputRadiance = builder.Read(input);
            data.inputVariance = builder.Read(reproject_pass->outputVariance);
            data.avgRadiance = builder.Read(reproject_pass->outputAvgRadiance);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputRadiance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser prefilter radiance");
            data.outputRadiance = builder.Write(data.outputRadiance);

            desc.format = GfxFormat::R16F;
            data.outputVariance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser prefilter variance");
            data.outputVariance = builder.Write(data.outputVariance);
        },
        [=](const PrefilterPassData& data, IGfxCommandList* pCommandList)
        {
            pCommandList->Dispatch(1, 1, 1);
        });

    struct ResovleTemporalPassData
    {
        RenderGraphHandle inputRadiance;
        RenderGraphHandle reprojectedRadiance;
        RenderGraphHandle avgRadiance;
        RenderGraphHandle inputVariance;
        RenderGraphHandle sampleCount;

        RenderGraphHandle outputRadiance;
        RenderGraphHandle outputVariance;
    };

    auto resolve_temporal_pass = pRenderGraph->AddPass<ResovleTemporalPassData>("ReflectionDenoiser - ResovleTemporal", RenderPassType::Compute,
        [&](ResovleTemporalPassData& data, RenderGraphBuilder& builder)
        {
            data.inputRadiance = builder.Read(prefilter_pass->outputRadiance);
            data.inputVariance = builder.Read(prefilter_pass->outputVariance);
            data.reprojectedRadiance = builder.Read(reproject_pass->outputRadiance);
            data.avgRadiance = builder.Read(reproject_pass->outputAvgRadiance);
            data.sampleCount = builder.Read(reproject_pass->outputSampleCount);

            data.outputRadiance = builder.Write(m_randianceHistory);
            data.outputVariance = builder.Write(m_varianceHistory);

            builder.MakeTarget(); //todo
        },
        [=](const ResovleTemporalPassData& data, IGfxCommandList* pCommandList)
        {
            pCommandList->Dispatch(1, 1, 1);
        });

    return reproject_pass->outputRadiance;
}

void ReflectionDenoiser::Reproject(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::Prefilter(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::ResolveTemporal(IGfxCommandList* pCommandList)
{
}
