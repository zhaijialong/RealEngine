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

    desc.cs = pRenderer->GetShader("restir_gi/denoiser_temporal.hlsl", "main", "cs_6_6", {});
    m_pTemporalFilterPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/temporal filter PSO");

    desc.cs = pRenderer->GetShader("restir_gi/denoiser_spatial.hlsl", "main", "cs_6_6", {});
    m_pSpatialFilterPSO = pRenderer->GetPipelineState(desc, "ReSTIR GI/spatial filter PSO");

    m_pDenoiser = eastl::make_unique<ReflectionDenoiser>(pRenderer);
}

RenderGraphHandle ReSTIRGI::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "ReSTIR GI (WIP)",
        [&]()
        {
            ImGui::Checkbox("Enable##ReSTIR GI", &m_bEnable);
            ImGui::SameLine(0.0f, 60.0f);
            ImGui::Checkbox("Second Bounce##ReSTIR GI", &m_bSecondBounce);
            ImGui::Checkbox("Enable ReSTIR##ReSTIR GI", &m_bEnableReSTIR);
            ImGui::Checkbox("Enable Denoiser##ReSTIR GI", &m_bEnableDenoiser);
        });

    if (!m_bEnable)
    {
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "ReSTIR GI");

    struct RaytracePassData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle outputRadiance; // w : hitT
        RenderGraphHandle outputHitNormal;
        RenderGraphHandle outputRay;
    };

    auto raytrace_pass = pRenderGraph->AddPass<RaytracePassData>("ReSTIR GI - initial sampling", RenderPassType::Compute,
        [&](RaytracePassData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputRadiance = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate radiance"));

            desc.format = GfxFormat::RG16UNORM;
            data.outputHitNormal = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate hitNormal"));
            data.outputRay = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate ray"));
        },
        [=](const RaytracePassData& data, IGfxCommandList* pCommandList)
        {
            InitialSampling(pCommandList, 
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.outputRadiance),
                pRenderGraph->GetTexture(data.outputHitNormal),
                pRenderGraph->GetTexture(data.outputRay),
                width, height);
        });

    if (!m_bEnableReSTIR)
    {
        return raytrace_pass->outputRadiance;
    }

    bool initialFrame = InitTemporalBuffers(width, height);
    eastl::swap(m_temporalReservoir[0], m_temporalReservoir[1]);

    struct TemporalReusePassData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle velocity;
        RenderGraphHandle prevLinearDepth;
        RenderGraphHandle prevNormal;

        RenderGraphHandle candidateRadiance;
        RenderGraphHandle candidateHitNormal;
        RenderGraphHandle candidateRay;

        RenderGraphHandle historyReservoirRayDirection;
        RenderGraphHandle historyReservoirSampleNormal;
        RenderGraphHandle historyReservoirSampleRadiance;
        RenderGraphHandle historyReservoir;

        RenderGraphHandle outputReservoirRayDirection;
        RenderGraphHandle outputReservoirSampleNormal;
        RenderGraphHandle outputReservoirSampleRadiance;
        RenderGraphHandle outputReservoir;
    };

    auto temporal_reuse_pass = pRenderGraph->AddPass<TemporalReusePassData>("ReSTIR GI - temporal resampling", RenderPassType::Compute,
        [&](TemporalReusePassData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.velocity = builder.Read(velocity);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevNormal = builder.Read(m_pRenderer->GetPrevNormalHandle());
            data.candidateRadiance = builder.Read(raytrace_pass->outputRadiance);
            data.candidateHitNormal = builder.Read(raytrace_pass->outputHitNormal);
            data.candidateRay = builder.Read(raytrace_pass->outputRay);

            GfxResourceState textureState = initialFrame ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS;

            data.historyReservoirRayDirection = builder.Read(builder.Import(m_temporalReservoir[0].rayDirection->GetTexture(), textureState));
            data.historyReservoirSampleNormal = builder.Read(builder.Import(m_temporalReservoir[0].sampleNormal->GetTexture(), textureState));
            data.historyReservoirSampleRadiance = builder.Read(builder.Import(m_temporalReservoir[0].sampleRadiance->GetTexture(), textureState));
            data.historyReservoir = builder.Read(builder.Import(m_temporalReservoir[0].reservoir->GetTexture(), textureState));

            data.outputReservoirRayDirection = builder.Write(builder.Import(m_temporalReservoir[1].rayDirection->GetTexture(), textureState));
            data.outputReservoirSampleNormal = builder.Write(builder.Import(m_temporalReservoir[1].sampleNormal->GetTexture(), textureState));
            data.outputReservoirSampleRadiance = builder.Write(builder.Import(m_temporalReservoir[1].sampleRadiance->GetTexture(), textureState));
            data.outputReservoir = builder.Write(builder.Import(m_temporalReservoir[1].reservoir->GetTexture(), textureState));
        },
        [=](const TemporalReusePassData& data, IGfxCommandList* pCommandList)
        {
            TemporalResampling(pCommandList,
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.candidateRadiance),
                pRenderGraph->GetTexture(data.candidateHitNormal),
                pRenderGraph->GetTexture(data.candidateRay),
                width, height);
        });

    struct SpatialReusePassData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;

        RenderGraphHandle inputReservoirRayDirection;
        RenderGraphHandle inputReservoirSampleNormal;
        RenderGraphHandle inputReservoirSampleRadiance;
        RenderGraphHandle inputReservoir;

        RenderGraphHandle outputReservoirRayDirection;
        RenderGraphHandle outputReservoirSampleRadiance;
        RenderGraphHandle outputReservoir;
    };

    auto spatial_reuse_pass = pRenderGraph->AddPass<SpatialReusePassData>("ReSTIR GI - spatial resampling", RenderPassType::Compute,
        [&](SpatialReusePassData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.inputReservoirRayDirection = builder.Read(temporal_reuse_pass->outputReservoirRayDirection);
            data.inputReservoirSampleNormal = builder.Read(temporal_reuse_pass->outputReservoirSampleNormal);
            data.inputReservoirSampleRadiance = builder.Read(temporal_reuse_pass->outputReservoirSampleRadiance);
            data.inputReservoir = builder.Read(temporal_reuse_pass->outputReservoir);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RG16UNORM;
            data.outputReservoirRayDirection = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/spatial reservoir ray"));

            desc.format = GfxFormat::R11G11B10F;
            data.outputReservoirSampleRadiance = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/spatial reservoir radiance"));

            desc.format = GfxFormat::RG16F;
            data.outputReservoir = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/spatial reservoir"));
        },
        [=](const SpatialReusePassData& data, IGfxCommandList* pCommandList)
        {
            SpatialResampling(pCommandList, 
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.outputReservoirRayDirection),
                pRenderGraph->GetTexture(data.outputReservoirSampleRadiance),
                pRenderGraph->GetTexture(data.outputReservoir),
                width, height);
        });

    struct ResolvePassData
    {
        RenderGraphHandle reservoir;
        RenderGraphHandle radiance;
        RenderGraphHandle rayDirection;
        RenderGraphHandle normal;
        RenderGraphHandle output;
    };

    auto resolve_pass = pRenderGraph->AddPass<ResolvePassData>("ReSTIR GI - resolve", RenderPassType::Compute,
        [&](ResolvePassData& data, RenderGraphBuilder& builder)
        {
            data.reservoir = builder.Read(spatial_reuse_pass->outputReservoir);
            data.radiance = builder.Read(spatial_reuse_pass->outputReservoirSampleRadiance);
            data.rayDirection = builder.Read(spatial_reuse_pass->outputReservoirRayDirection);
            data.normal = builder.Read(normal);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/resolve output"));
        },
        [=](const ResolvePassData& data, IGfxCommandList* pCommandList)
        {
            Resolve(pCommandList, 
                pRenderGraph->GetTexture(data.reservoir),
                pRenderGraph->GetTexture(data.radiance),
                pRenderGraph->GetTexture(data.rayDirection),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    if (!m_bEnableDenoiser)
    {
        return resolve_pass->output;
    }

    /*
    struct TemporalFilterPassData
    {
        RenderGraphHandle input;
        RenderGraphHandle history;
        RenderGraphHandle output;
    };

    auto temporal_filter_pass = pRenderGraph->AddPass<TemporalFilterPassData>("ReSTIR GI - temporal filter", RenderPassType::Compute,
        [&](TemporalFilterPassData& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(resolve_pass->output);
            data.history = builder.Read(builder.Import(m_pHistoryRadiance->GetTexture(), GfxResourceState::UnorderedAccess));

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/temporal filter output"));
        },
        [=](const TemporalFilterPassData& data, IGfxCommandList* pCommandList)
        {
            TemporalFilter(pCommandList,
                pRenderGraph->GetTexture(data.input),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    struct SpatialFilterPassData
    {
        RenderGraphHandle input;
        RenderGraphHandle output;
        RenderGraphHandle outputHistory;
    };

    auto spatial_filter_pass = pRenderGraph->AddPass<SpatialFilterPassData>("ReSTIR GI - spatial filter", RenderPassType::Compute,
        [&](SpatialFilterPassData& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(temporal_filter_pass->output);
            data.outputHistory = builder.Write(temporal_filter_pass->history);
            
            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/spatial filter output"));
        },
        [=](const SpatialFilterPassData& data, IGfxCommandList* pCommandList)
        {
            SpatialFilter(pCommandList,
                pRenderGraph->GetTexture(data.input),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    return spatial_filter_pass->output;
    */

    struct PrepareDenoiserArgsPassData
    {
        RenderGraphHandle argsBuffer;
        RenderGraphHandle tileListBuffer;
    };

    auto prepare_args_pass = pRenderGraph->AddPass<PrepareDenoiserArgsPassData>("ReSTIR GI - prepare denoiser args", RenderPassType::Copy,
        [&](PrepareDenoiserArgsPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.argsBuffer = builder.Create<RenderGraphBuffer>(desc, "ReSTIR GI/denoiser indirect args");
            data.argsBuffer = builder.Write(data.argsBuffer);

            uint32_t tile_count_x = (width + 7) / 8;
            uint32_t tile_count_y = (height + 7) / 8;
            desc.size = sizeof(uint32_t) * tile_count_x * tile_count_y;
            data.tileListBuffer = builder.Create<RenderGraphBuffer>(desc, "ReSTIR GI/denoiser tile list");
            data.tileListBuffer = builder.Write(data.tileListBuffer);
        },
        [=](const PrepareDenoiserArgsPassData& data, IGfxCommandList* pCommandList)
        {
            IGfxBuffer* argsBuffer = pRenderGraph->GetBuffer(data.argsBuffer)->GetBuffer();
            IGfxBuffer* tileListBuffer = pRenderGraph->GetBuffer(data.tileListBuffer)->GetBuffer();

            uint32_t tile_count_x = (width + 7) / 8;
            uint32_t tile_count_y = (height + 7) / 8;
            pCommandList->WriteBuffer(argsBuffer, 0, tile_count_x * tile_count_y);
            pCommandList->WriteBuffer(argsBuffer, 4, 1);
            pCommandList->WriteBuffer(argsBuffer, 8, 1);

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
        });

    m_pDenoiser->ImportTextures(pRenderGraph, width, height);
    return m_pDenoiser->Render(pRenderGraph, prepare_args_pass->argsBuffer, prepare_args_pass->tileListBuffer,
        resolve_pass->output, depth, linear_depth, normal, velocity, width, height, 1.0f, 0.3f);
}

void ReSTIRGI::InitialSampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal,
    RenderGraphTexture* outputRadiance, RenderGraphTexture* outputHitNormal, RenderGraphTexture* outputRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pInitialSamplingPSO);

    uint32_t constants[5] = { 
        depth->GetSRV()->GetHeapIndex(), 
        normal->GetSRV()->GetHeapIndex(),
        outputRadiance->GetUAV()->GetHeapIndex(),
        outputHitNormal->GetUAV()->GetHeapIndex(),
        outputRay->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::TemporalResampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal, RenderGraphTexture* velocity, 
    RenderGraphTexture* candidateRadiance, RenderGraphTexture* candidateHitNormal, RenderGraphTexture* candidateRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalResamplingPSO);

    struct CB
    {
        uint depth;
        uint normal;
        uint velocity;
        uint prevLinearDepth;
        uint prevNormal;
        uint candidateRadiance;
        uint candidateHitNormal;
        uint candidateRay;
        uint historyReservoirRayDirection;
        uint historyReservoirSampleNormal;
        uint historyReservoirSampleRadiance;
        uint historyReservoir;
        uint outputReservoirRayDirection;
        uint outputReservoirSampleNormal;
        uint outputReservoirSampleRadiance;
        uint outputReservoir;
    };

    CB cb;
    cb.depth = depth->GetSRV()->GetHeapIndex();
    cb.normal = normal->GetSRV()->GetHeapIndex();
    cb.velocity = velocity->GetSRV()->GetHeapIndex();
    cb.prevLinearDepth = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.prevNormal = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
    cb.candidateRadiance = candidateRadiance->GetSRV()->GetHeapIndex();
    cb.candidateHitNormal = candidateHitNormal->GetSRV()->GetHeapIndex();
    cb.candidateRay = candidateRay->GetSRV()->GetHeapIndex();
    cb.historyReservoirRayDirection = m_temporalReservoir[0].rayDirection->GetSRV()->GetHeapIndex();
    cb.historyReservoirSampleNormal = m_temporalReservoir[0].sampleNormal->GetSRV()->GetHeapIndex();
    cb.historyReservoirSampleRadiance = m_temporalReservoir[0].sampleRadiance->GetSRV()->GetHeapIndex();
    cb.historyReservoir = m_temporalReservoir[0].reservoir->GetSRV()->GetHeapIndex();
    cb.outputReservoirRayDirection = m_temporalReservoir[1].rayDirection->GetUAV()->GetHeapIndex();
    cb.outputReservoirSampleNormal = m_temporalReservoir[1].sampleNormal->GetUAV()->GetHeapIndex();
    cb.outputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetUAV()->GetHeapIndex();
    cb.outputReservoir = m_temporalReservoir[1].reservoir->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::SpatialResampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal,
    RenderGraphTexture* outputReservoirRayDirection, RenderGraphTexture* outputReservoirSampleRadiance, RenderGraphTexture* outputReservoir, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pSpatialResamplingPSO);

    struct Constants
    {
        uint depth;
        uint normal;
        uint inputReservoirRayDirection;
        uint inputReservoirSampleNormal;
        uint inputReservoirSampleRadiance;
        uint inputReservoir;
        uint outputReservoirRayDirection;
        uint outputReservoirSampleRadiance;
        uint outputReservoir;
    };

    Constants constants;
    constants.depth = depth->GetSRV()->GetHeapIndex();
    constants.normal = normal->GetSRV()->GetHeapIndex();
    constants.inputReservoirRayDirection = m_temporalReservoir[1].rayDirection->GetSRV()->GetHeapIndex();
    constants.inputReservoirSampleNormal = m_temporalReservoir[1].sampleNormal->GetSRV()->GetHeapIndex();
    constants.inputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetSRV()->GetHeapIndex();
    constants.inputReservoir = m_temporalReservoir[1].reservoir->GetSRV()->GetHeapIndex();
    constants.outputReservoirRayDirection = outputReservoirRayDirection->GetUAV()->GetHeapIndex();
    constants.outputReservoirSampleRadiance = outputReservoirSampleRadiance->GetUAV()->GetHeapIndex();
    constants.outputReservoir = outputReservoir->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::Resolve(IGfxCommandList* pCommandList, RenderGraphTexture* reservoir, RenderGraphTexture* radiance, RenderGraphTexture* rayDirection, RenderGraphTexture* normal,
    RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolvePSO);

    uint32_t constants[5] = { 
        reservoir->GetSRV()->GetHeapIndex(),
        radiance->GetSRV()->GetHeapIndex(),
        rayDirection->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
/*
void ReSTIRGI::TemporalFilter(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalFilterPSO);

    uint32_t constants[3] = {
        input->GetSRV()->GetHeapIndex(),
        m_pHistoryRadiance->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::SpatialFilter(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pSpatialFilterPSO);

    uint32_t constants[3] = {
        input->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(),
        m_pHistoryRadiance->GetUAV()->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
*/

bool ReSTIRGI::InitTemporalBuffers(uint32_t width, uint32_t height)
{
    bool initialFrame = false;

    if (m_temporalReservoir[0].reservoir == nullptr ||
        m_temporalReservoir[0].reservoir->GetTexture()->GetDesc().width != width ||
        m_temporalReservoir[0].reservoir->GetTexture()->GetDesc().height != height)
    {
        for (int i = 0; i < 2; ++i)
        {
            m_temporalReservoir[i].rayDirection.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir rayDirection {}", i).c_str()));
            m_temporalReservoir[i].sampleNormal.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleNormal {}", i).c_str()));
            m_temporalReservoir[i].sampleRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleRadiance {}", i).c_str()));
            m_temporalReservoir[i].reservoir.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir {}", i).c_str()));
        }

        //m_pHistoryRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess,
        //    "ReSTIR GI/history radiance"));

        initialFrame = true;
    }

    return initialFrame;
}
