#include "restir_gi.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "fmt/format.h"

ReSTIRGI::ReSTIRGI(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("restir_gi/initial_sampling.hlsl", "main", "cs_6_6", {});
    m_pInitialSamplingPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/initial sampling PSO");

    desc.cs = pRenderer->GetShader("restir_gi/temporal_resampling.hlsl", "main", "cs_6_6", {});
    m_pTemporalResamplingPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/temporal resampling PSO");

    desc.cs = pRenderer->GetShader("restir_gi/spatial_resampling.hlsl", "main", "cs_6_6", {});
    m_pSpatialResamplingPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/spatial resampling PSO");

    desc.cs = pRenderer->GetShader("restir_gi/restir_resolve.hlsl", "main", "cs_6_6", {});
    m_pResolvePSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/resolve PSO");

    m_pDenoiser = eastl::make_unique<ReflectionDenoiser>(pRenderer);
}

RGHandle ReSTIRGI::Render(RenderGraph* pRenderGraph, RGHandle halfDepthNormal, RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "ReSTIR GI (WIP)",
        [&]()
        {
            ImGui::Checkbox("Enable##ReSTIR GI", &m_bEnable);
            ImGui::Checkbox("Enable ReSTIR##ReSTIR GI", &m_bEnableReSTIR);
            ImGui::Checkbox("Enable Denoiser##ReSTIR GI", &m_bEnableDenoiser);
        });

    if (!m_bEnable)
    {
        return RGHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "ReSTIR GI");

    uint32_t half_width = (width + 1) / 2;
    uint32_t half_height = (height + 1) / 2;

    m_pDenoiser->ImportTextures(pRenderGraph, width, height);

    struct RaytracePassData
    {
        RGHandle halfDepthNormal;
        RGHandle prevLinearDepth;
        RGHandle historyRadiance;
        RGHandle outputRadiance;
    };

    auto raytrace_pass = pRenderGraph->AddPass<RaytracePassData>("ReSTIR GI - initial sampling", RenderPassType::Compute,
        [&](RaytracePassData& data, RGBuilder& builder)
        {
            data.halfDepthNormal = builder.Read(halfDepthNormal);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.historyRadiance = builder.Read(m_pDenoiser->GetHistoryRadiance());

            RGTexture::Desc desc;
            desc.width = half_width;
            desc.height = half_height;
            desc.format = GfxFormat::R11G11B10F;
            data.outputRadiance = builder.Write(builder.Create<RGTexture>(desc, "ReSTIR GI/candidate radiance"));
        },
        [=](const RaytracePassData& data, IGfxCommandList* pCommandList)
        {
            InitialSampling(pCommandList, 
                pRenderGraph->GetTexture(data.halfDepthNormal),
                pRenderGraph->GetTexture(data.outputRadiance),
                half_width, half_height);
        });

    if (!m_bEnableReSTIR)
    {
        return raytrace_pass->outputRadiance;
    }

    bool initialFrame = InitTemporalBuffers(half_width, half_height);
    eastl::swap(m_temporalReservoir[0], m_temporalReservoir[1]);

    struct TemporalReusePassData
    {
        RGHandle halfDepthNormal;
        RGHandle velocity;
        RGHandle prevLinearDepth;
        RGHandle prevNormal;
        RGHandle candidateRadiance;
        RGHandle historyReservoirSampleRadiance;
        RGHandle historyReservoir;
        RGHandle outputReservoirSampleRadiance;
        RGHandle outputReservoir;
    };

    auto temporal_reuse_pass = pRenderGraph->AddPass<TemporalReusePassData>("ReSTIR GI - temporal resampling", RenderPassType::Compute,
        [&](TemporalReusePassData& data, RGBuilder& builder)
        {
            data.halfDepthNormal = builder.Read(halfDepthNormal);
            data.velocity = builder.Read(velocity);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevNormal = builder.Read(m_pRenderer->GetPrevNormalHandle());
            data.candidateRadiance = builder.Read(raytrace_pass->outputRadiance);

            GfxResourceState textureState = initialFrame ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS;

            data.historyReservoirSampleRadiance = builder.Read(builder.Import(m_temporalReservoir[0].sampleRadiance->GetTexture(), textureState));
            data.historyReservoir = builder.Read(builder.Import(m_temporalReservoir[0].reservoir->GetTexture(), textureState));

            data.outputReservoirSampleRadiance = builder.Write(builder.Import(m_temporalReservoir[1].sampleRadiance->GetTexture(), textureState));
            data.outputReservoir = builder.Write(builder.Import(m_temporalReservoir[1].reservoir->GetTexture(), textureState));
        },
        [=](const TemporalReusePassData& data, IGfxCommandList* pCommandList)
        {
            TemporalResampling(pCommandList,
                pRenderGraph->GetTexture(data.halfDepthNormal),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.candidateRadiance),
                half_width, half_height);
        });

    struct SpatialReusePassData
    {
        RGHandle halfDepthNormal;

        RGHandle inputReservoirSampleRadiance;
        RGHandle inputReservoir;

        RGHandle outputReservoirSampleRadiance;
        RGHandle outputReservoir;
    };

    auto spatial_reuse_pass = pRenderGraph->AddPass<SpatialReusePassData>("ReSTIR GI - spatial resampling", RenderPassType::Compute,
        [&](SpatialReusePassData& data, RGBuilder& builder)
        {
            data.halfDepthNormal = builder.Read(halfDepthNormal);
            data.inputReservoirSampleRadiance = builder.Read(temporal_reuse_pass->outputReservoirSampleRadiance);
            data.inputReservoir = builder.Read(temporal_reuse_pass->outputReservoir);

            RGTexture::Desc desc;
            desc.width = half_width;
            desc.height = half_height;
            desc.format = GfxFormat::R11G11B10F;
            data.outputReservoirSampleRadiance = builder.Write(builder.Create<RGTexture>(desc, "ReSTIR GI/spatial reservoir radiance"));

            desc.format = GfxFormat::RG16F;
            data.outputReservoir = builder.Write(builder.Create<RGTexture>(desc, "ReSTIR GI/spatial reservoir"));
        },
        [=](const SpatialReusePassData& data, IGfxCommandList* pCommandList)
        {
            SpatialResampling(pCommandList, 
                pRenderGraph->GetTexture(data.halfDepthNormal),
                pRenderGraph->GetTexture(data.outputReservoirSampleRadiance),
                pRenderGraph->GetTexture(data.outputReservoir),
                half_width, half_height);
        });

    struct ResolvePassData
    {
        RGHandle reservoir;
        RGHandle radiance;
        RGHandle normal;
        RGHandle output;
    };

    auto resolve_pass = pRenderGraph->AddPass<ResolvePassData>("ReSTIR GI - resolve", RenderPassType::Compute,
        [&](ResolvePassData& data, RGBuilder& builder)
        {
            data.reservoir = builder.Read(spatial_reuse_pass->outputReservoir);
            data.radiance = builder.Read(spatial_reuse_pass->outputReservoirSampleRadiance);
            data.normal = builder.Read(normal);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "ReSTIR GI/resolve output"));
        },
        [=](const ResolvePassData& data, IGfxCommandList* pCommandList)
        {
            Resolve(pCommandList, 
                pRenderGraph->GetTexture(data.reservoir),
                pRenderGraph->GetTexture(data.radiance),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    if (!m_bEnableDenoiser)
    {
        return resolve_pass->output;
    }

    struct PrepareDenoiserArgsPassData
    {
        RGHandle argsBuffer;
        RGHandle tileListBuffer;
    };

    auto prepare_args_pass = pRenderGraph->AddPass<PrepareDenoiserArgsPassData>("ReSTIR GI - prepare denoiser args", RenderPassType::Copy,
        [&](PrepareDenoiserArgsPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.argsBuffer = builder.Create<RGBuffer>(desc, "ReSTIR GI/denoiser indirect args");
            data.argsBuffer = builder.Write(data.argsBuffer);

            uint32_t tile_count_x = (width + 7) / 8;
            uint32_t tile_count_y = (height + 7) / 8;
            desc.size = sizeof(uint32_t) * tile_count_x * tile_count_y;
            data.tileListBuffer = builder.Create<RGBuffer>(desc, "ReSTIR GI/denoiser tile list");
            data.tileListBuffer = builder.Write(data.tileListBuffer);
        },
        [=](const PrepareDenoiserArgsPassData& data, IGfxCommandList* pCommandList)
        {
            IGfxBuffer* argsBuffer = pRenderGraph->GetBuffer(data.argsBuffer)->GetBuffer();
            IGfxBuffer* tileListBuffer = pRenderGraph->GetBuffer(data.tileListBuffer)->GetBuffer();

            uint32_t tile_count_x = (width + 7) / 8;
            uint32_t tile_count_y = (height + 7) / 8;

            {
                StagingBuffer staging_buffer = m_pRenderer->GetStagingBufferAllocator()->Allocate(sizeof(uint3));
                char* dst_data = (char*)staging_buffer.buffer->GetCpuAddress() + staging_buffer.offset;

                uint3 dispatchArgs(tile_count_x * tile_count_y, 1, 1);
                memcpy(dst_data, &dispatchArgs, sizeof(uint3));

                pCommandList->CopyBuffer(argsBuffer, 0, staging_buffer.buffer, staging_buffer.offset, sizeof(uint3));
            }

            {
                eastl::vector<uint32_t> tileList;
                tileList.reserve(tile_count_x * tile_count_y);

                for (uint32_t x = 0; x < tile_count_x; ++x)
                {
                    for (uint32_t y = 0; y < tile_count_y; ++y)
                    {
                        uint32_t tile_coord_x = x * 8;
                        uint32_t tile_coord_y = y * 8;
                        uint32_t tile = ((tile_coord_y & 0xffffu) << 16) | ((tile_coord_x & 0xffffu) << 0);
                        tileList.push_back(tile);
                    }
                }

                uint32_t data_size = sizeof(uint32_t) * tile_count_x * tile_count_y;
                StagingBuffer staging_buffer = m_pRenderer->GetStagingBufferAllocator()->Allocate(data_size);
                char* dst_data = (char*)staging_buffer.buffer->GetCpuAddress() + staging_buffer.offset;
                memcpy(dst_data, tileList.data(), data_size);

                pCommandList->CopyBuffer(tileListBuffer, 0, staging_buffer.buffer, staging_buffer.offset, data_size);
            }
        });

    return m_pDenoiser->Render(pRenderGraph, prepare_args_pass->argsBuffer, prepare_args_pass->tileListBuffer,
        resolve_pass->output, depth, linear_depth, normal, velocity, width, height, 1.0f, 1.0f);
}

void ReSTIRGI::InitialSampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal, RGTexture* outputRadiance, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pInitialSamplingPSO);

    struct CB
    {
        uint halfDepthNormalTexture;
        uint prevLinearDepthTexture;
        uint historyRadiance;
        uint outputRadianceUAV;
    };

    CB constants;
    constants.halfDepthNormalTexture = halfDepthNormal->GetSRV()->GetHeapIndex();
    constants.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    constants.historyRadiance = m_pDenoiser->GetHistoryRadianceSRV()->GetHeapIndex();
    constants.outputRadianceUAV = outputRadiance->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::TemporalResampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal, RGTexture* velocity, RGTexture* candidateRadiance, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalResamplingPSO);

    struct CB
    {
        uint halfDepthNormal;
        uint velocity;
        uint prevLinearDepth;
        uint prevNormal;
        uint candidateRadiance;
        uint historyReservoirSampleRadiance;
        uint historyReservoir;
        uint outputReservoirSampleRadiance;
        uint outputReservoir;
    };

    CB cb;
    cb.halfDepthNormal = halfDepthNormal->GetSRV()->GetHeapIndex();
    cb.velocity = velocity->GetSRV()->GetHeapIndex();
    cb.prevLinearDepth = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.prevNormal = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
    cb.candidateRadiance = candidateRadiance->GetSRV()->GetHeapIndex();
    cb.historyReservoirSampleRadiance = m_temporalReservoir[0].sampleRadiance->GetSRV()->GetHeapIndex();
    cb.historyReservoir = m_temporalReservoir[0].reservoir->GetSRV()->GetHeapIndex();
    cb.outputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetUAV()->GetHeapIndex();
    cb.outputReservoir = m_temporalReservoir[1].reservoir->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::SpatialResampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal, RGTexture* outputReservoirSampleRadiance, RGTexture* outputReservoir, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pSpatialResamplingPSO);

    struct Constants
    {
        uint halfDepthNormal;
        uint inputReservoirSampleRadiance;
        uint inputReservoir;
        uint outputReservoirSampleRadiance;
        uint outputReservoir;
    };

    Constants constants;
    constants.halfDepthNormal = halfDepthNormal->GetSRV()->GetHeapIndex();
    constants.inputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetSRV()->GetHeapIndex();
    constants.inputReservoir = m_temporalReservoir[1].reservoir->GetSRV()->GetHeapIndex();
    constants.outputReservoirSampleRadiance = outputReservoirSampleRadiance->GetUAV()->GetHeapIndex();
    constants.outputReservoir = outputReservoir->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::Resolve(IGfxCommandList* pCommandList, RGTexture* reservoir, RGTexture* radiance, RGTexture* normal,
    RGTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolvePSO);

    uint32_t constants[4] = { 
        reservoir->GetSRV()->GetHeapIndex(),
        radiance->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

bool ReSTIRGI::InitTemporalBuffers(uint32_t width, uint32_t height)
{
    bool initialFrame = false;

    if (m_temporalReservoir[0].reservoir == nullptr ||
        m_temporalReservoir[0].reservoir->GetTexture()->GetDesc().width != width ||
        m_temporalReservoir[0].reservoir->GetTexture()->GetDesc().height != height)
    {
        for (int i = 0; i < 2; ++i)
        {
            m_temporalReservoir[i].sampleRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleRadiance {}", i).c_str()));
            m_temporalReservoir[i].reservoir.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir {}", i).c_str()));
        }

        initialFrame = true;
    }

    return initialFrame;
}
