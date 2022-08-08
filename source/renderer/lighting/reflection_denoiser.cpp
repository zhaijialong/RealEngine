#include "reflection_denoiser.h"
#include "../renderer.h"

ReflectionDenoiser::ReflectionDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxShaderCompilerFlags flags = 0;
#ifdef _DEBUG
    if (m_pRenderer->GetDevice()->GetVendor() == GfxVendor::AMD)
    {
        flags = GfxShaderCompilerFlagO1; //todo : AMD crashes with -O0
    }
#endif

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_reproject.hlsl", "main", "cs_6_6", {}, flags);
    m_pReprojectPSO = pRenderer->GetPipelineState(desc, "HSR denoiser reproject PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_prefilter.hlsl", "main", "cs_6_6", {});
    m_pPrefilterPSO = pRenderer->GetPipelineState(desc, "HSR denoiser prefilter PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_resolve_temporal.hlsl", "main", "cs_6_6", {});
    m_pResolveTemporalPSO = pRenderer->GetPipelineState(desc, "HSR denoiser resolve temporal PSO");
}

void ReflectionDenoiser::ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pRadianceHistory == nullptr ||
        m_pRadianceHistory->GetTexture()->GetDesc().width != width ||
        m_pRadianceHistory->GetTexture()->GetDesc().height != height)
    {
        m_bHistoryInvalid = true;
        m_pRadianceHistory.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pRadianceHistory"));
        m_pVarianceHistory.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pVarianceHistory"));

        m_pSampleCountInput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pSampleCount0"));
        m_pSampleCountOutput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "ReflectionDenoiser::m_pSampleCount1"));
    }

    eastl::swap(m_pSampleCountInput, m_pSampleCountOutput);

    m_radianceHistory = pRenderGraph->Import(m_pRadianceHistory->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
    m_varianceHistory = pRenderGraph->Import(m_pVarianceHistory->GetTexture(), GfxResourceState::UnorderedAccess);
    m_sampleCountInput = pRenderGraph->Import(m_pSampleCountInput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
    m_sampleCountOutput = pRenderGraph->Import(m_pSampleCountOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RenderGraphHandle ReflectionDenoiser::Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, 
    RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height,
    float maxRoughness, float temporalStability)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "ReflectionDenoiser");

    if (m_bHistoryInvalid)
    {
        struct ClearHistoryPassData
        {
            RenderGraphHandle radianceHistory;
            RenderGraphHandle varianceHistory;
            RenderGraphHandle sampleCountInput;
            RenderGraphHandle sampleCountOutput;
        };

        auto clear_pass = pRenderGraph->AddPass<ClearHistoryPassData>("ReflectionDenoiser - Clear", RenderPassType::Compute,
            [&](ClearHistoryPassData& data, RenderGraphBuilder& builder)
            {
                data.radianceHistory = builder.Write(m_radianceHistory);
                data.varianceHistory = builder.Write(m_varianceHistory);
                data.sampleCountInput = builder.Write(m_sampleCountInput);
                data.sampleCountOutput = builder.Write(m_sampleCountOutput);
            },
            [=](const ClearHistoryPassData& data, IGfxCommandList* pCommandList)
            {
                float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                pCommandList->ClearUAV(m_pRadianceHistory->GetTexture(), m_pRadianceHistory->GetUAV(), clear_value);
                pCommandList->ClearUAV(m_pVarianceHistory->GetTexture(), m_pVarianceHistory->GetUAV(), clear_value);
                pCommandList->ClearUAV(m_pSampleCountInput->GetTexture(), m_pSampleCountInput->GetUAV(), clear_value);
                pCommandList->ClearUAV(m_pSampleCountOutput->GetTexture(), m_pSampleCountOutput->GetUAV(), clear_value);
                pCommandList->UavBarrier(m_pSampleCountOutput->GetTexture());
                m_bHistoryInvalid = false;
            });

        m_radianceHistory = clear_pass->radianceHistory;
        m_varianceHistory = clear_pass->varianceHistory;
        m_sampleCountInput = clear_pass->sampleCountInput;
        m_sampleCountOutput = clear_pass->sampleCountOutput;
    }

    struct ReprojectPassData
    {
        RenderGraphHandle indirectArgs;
        RenderGraphHandle tileListBuffer;

        RenderGraphHandle depth;
        RenderGraphHandle linearDepth;
        RenderGraphHandle normal;
        RenderGraphHandle velocity;
        RenderGraphHandle prevLinearDepth;
        RenderGraphHandle prevNormal;

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
            
            data.depth = builder.Read(depth);
            data.linearDepth = builder.Read(linear_depth);
            data.normal = builder.Read(normal);
            data.velocity = builder.Read(velocity);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevNormal = builder.Read(m_pRenderer->GetPrevNormalHandle());

            data.inputRadiance = builder.Read(input);
            data.historyRadiance = builder.Read(m_radianceHistory);
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
            data.outputAvgRadiance = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser average radiance");
            data.outputAvgRadiance = builder.Write(data.outputAvgRadiance);

            data.outputSampleCount = builder.Write(m_sampleCountOutput);
        },
        [=](const ReprojectPassData& data, IGfxCommandList* pCommandList)
        {
            float root_constants[2] = { maxRoughness, temporalStability };
            pCommandList->SetComputeConstants(0, root_constants, sizeof(root_constants));

            Reproject(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectArgs),
                pRenderGraph->GetBuffer(data.tileListBuffer),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.linearDepth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.inputRadiance),
                pRenderGraph->GetTexture(data.outputRadiance),
                pRenderGraph->GetTexture(data.outputVariance),
                pRenderGraph->GetTexture(data.outputAvgRadiance));
        });

    struct PrefilterPassData
    {
        RenderGraphHandle indirectArgs;
        RenderGraphHandle tileListBuffer;

        RenderGraphHandle linearDepth;
        RenderGraphHandle normal;

        RenderGraphHandle inputRadiance;
        RenderGraphHandle inputVariance;
        RenderGraphHandle avgRadiance;

        RenderGraphHandle outputRadiance;
        RenderGraphHandle outputVariance;
    };

    auto prefilter_pass = pRenderGraph->AddPass<PrefilterPassData>("ReflectionDenoiser - Prefilter", RenderPassType::Compute,
        [&](PrefilterPassData& data, RenderGraphBuilder& builder)
        {
            data.indirectArgs = builder.ReadIndirectArg(indirectArgs);
            data.tileListBuffer = builder.Read(tileListBuffer);
            data.linearDepth = builder.Read(linear_depth);
            data.normal = builder.Read(normal);

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
            Prefilter(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectArgs),
                pRenderGraph->GetBuffer(data.tileListBuffer),
                pRenderGraph->GetTexture(data.linearDepth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.inputRadiance),
                pRenderGraph->GetTexture(data.inputVariance),
                pRenderGraph->GetTexture(data.avgRadiance),
                pRenderGraph->GetTexture(data.outputRadiance),
                pRenderGraph->GetTexture(data.outputVariance));
        });

    struct ResovleTemporalPassData
    {
        RenderGraphHandle indirectArgs;
        RenderGraphHandle tileListBuffer;

        RenderGraphHandle normal;
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
            data.indirectArgs = builder.ReadIndirectArg(indirectArgs);
            data.tileListBuffer = builder.Read(tileListBuffer);

            data.normal = builder.Read(normal);
            data.inputRadiance = builder.Read(prefilter_pass->outputRadiance);
            data.inputVariance = builder.Read(prefilter_pass->outputVariance);
            data.reprojectedRadiance = builder.Read(reproject_pass->outputRadiance);
            data.avgRadiance = builder.Read(reproject_pass->outputAvgRadiance);
            data.sampleCount = builder.Read(reproject_pass->outputSampleCount);

            data.outputRadiance = builder.Write(m_radianceHistory);
            data.outputVariance = builder.Write(m_varianceHistory);
        },
        [=](const ResovleTemporalPassData& data, IGfxCommandList* pCommandList)
        {
			ResolveTemporal(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectArgs), 
                pRenderGraph->GetBuffer(data.tileListBuffer),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.inputRadiance),
                pRenderGraph->GetTexture(data.inputVariance),
                pRenderGraph->GetTexture(data.reprojectedRadiance),
                pRenderGraph->GetTexture(data.avgRadiance));
        });

    return resolve_temporal_pass->outputRadiance;
}

void ReflectionDenoiser::Reproject(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
    RenderGraphTexture* depth, RenderGraphTexture* linearDepth, RenderGraphTexture* normal, RenderGraphTexture* velocity, RenderGraphTexture* inputRadiance,
    RenderGraphTexture* outputRadianceUAV, RenderGraphTexture* outputVariancceUAV, RenderGraphTexture* outputAvgRadianceUAV)
{
    float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCommandList->ClearUAV(outputAvgRadianceUAV->GetTexture(), outputAvgRadianceUAV->GetUAV(), clear_value);
    pCommandList->UavBarrier(outputAvgRadianceUAV->GetTexture());

    pCommandList->SetPipelineState(m_pReprojectPSO);

    struct Constants
    {
        uint tileListBuffer;

        uint depthTexture;
        uint normalTexture;
        uint velocityTexture;
        uint prevLinearDepthTexture;
        uint prevNormalTexture;

        uint inputRadianceTexture;
        uint historyRadianceTexture;
        uint historyVarianceTexture;
        uint historySampleNumTexture;

        uint outputRadianceUAV;
        uint outputVarianceUAV;
        uint outputAvgRadianceUAV;
        uint outputSampleNumUAV;
    };

    Constants constants;
    constants.tileListBuffer = tileList->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.velocityTexture = velocity->GetSRV()->GetHeapIndex();
    if (m_pRenderer->IsHistoryTextureValid())
    {
        constants.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
        constants.prevNormalTexture = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
        constants.historyRadianceTexture = m_pRadianceHistory->GetSRV()->GetHeapIndex();
    }
    else
    {
        constants.prevLinearDepthTexture = linearDepth->GetSRV()->GetHeapIndex();
        constants.prevNormalTexture = normal->GetSRV()->GetHeapIndex();
        constants.historyRadianceTexture = inputRadiance->GetSRV()->GetHeapIndex();
    }
    constants.inputRadianceTexture = inputRadiance->GetSRV()->GetHeapIndex();
    constants.historyVarianceTexture = m_pVarianceHistory->GetSRV()->GetHeapIndex();
    constants.historySampleNumTexture = m_pSampleCountInput->GetSRV()->GetHeapIndex();
    constants.outputRadianceUAV = outputRadianceUAV->GetUAV()->GetHeapIndex();
    constants.outputVarianceUAV = outputVariancceUAV->GetUAV()->GetHeapIndex();
    constants.outputAvgRadianceUAV = outputAvgRadianceUAV->GetUAV()->GetHeapIndex();
    constants.outputSampleNumUAV = m_pSampleCountOutput->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
}

void ReflectionDenoiser::Prefilter(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
    RenderGraphTexture* linear_depth, RenderGraphTexture* normal, RenderGraphTexture* inputRadiance, RenderGraphTexture* inputVariance, RenderGraphTexture* avgRadiance,
    RenderGraphTexture* outputRadianceUAV, RenderGraphTexture* outputVariancceUAV)
{
    pCommandList->SetPipelineState(m_pPrefilterPSO);

    struct Constants
    {
        uint tileListBuffer;
        uint linearDepthTexture;
        uint normalTexture;
        uint radianceTexture;
        uint varianceTexture;
        uint avgRadianceTexture;
        uint outputRadianceUAV;
        uint outputVarianceUAV;
    };

    Constants constants;
    constants.tileListBuffer = tileList->GetSRV()->GetHeapIndex();
    constants.linearDepthTexture = linear_depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.radianceTexture = inputRadiance->GetSRV()->GetHeapIndex();
    constants.varianceTexture = inputVariance->GetSRV()->GetHeapIndex();
    constants.avgRadianceTexture = avgRadiance->GetSRV()->GetHeapIndex();
    constants.outputRadianceUAV = outputRadianceUAV->GetUAV()->GetHeapIndex();
    constants.outputVarianceUAV = outputVariancceUAV->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
}

void ReflectionDenoiser::ResolveTemporal(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
    RenderGraphTexture* normal, RenderGraphTexture* inputRadiance, RenderGraphTexture* inputVariance, RenderGraphTexture* reprojectedRadiance, RenderGraphTexture* avgRadiance)
{
    pCommandList->SetPipelineState(m_pResolveTemporalPSO);

    struct Constants
    {
        uint tileListBuffer;
        uint normalTexture;
        uint inputRadianceTexture;
        uint reprojectedRadianceTexture;
        uint avgRadianceTexture;
        uint inputVarianceTexture;
        uint sampleCountTexture;
        uint outputRadianceUAV;
        uint outputVarianceUAV;
    };

    Constants constants;
    constants.tileListBuffer = tileList->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.inputRadianceTexture = inputRadiance->GetSRV()->GetHeapIndex();
    constants.reprojectedRadianceTexture = reprojectedRadiance->GetSRV()->GetHeapIndex();
    constants.avgRadianceTexture = avgRadiance->GetSRV()->GetHeapIndex();
    constants.inputVarianceTexture = inputVariance->GetSRV()->GetHeapIndex();
    constants.sampleCountTexture = m_pSampleCountOutput->GetSRV()->GetHeapIndex();
    constants.outputRadianceUAV = m_pRadianceHistory->GetUAV()->GetHeapIndex();
    constants.outputVarianceUAV = m_pVarianceHistory->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
}
