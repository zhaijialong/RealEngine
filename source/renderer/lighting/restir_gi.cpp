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
}

RenderGraphHandle ReSTIRGI::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "ReSTIR GI",
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
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* outputRadiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRadiance);
            RenderGraphTexture* outputHitNormal = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputHitNormal);
            RenderGraphTexture* outputRay = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRay);
            InitialSampling(pCommandList, depth->GetSRV(), normal->GetSRV(), outputRadiance->GetUAV(), outputHitNormal->GetUAV(), outputRay->GetUAV(), width, height);
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
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* velocity = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* candidateRadiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateRadiance);
            RenderGraphTexture* candidateHitNormal = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateHitNormal);
            RenderGraphTexture* candidateRay = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateRay);
            TemporalResampling(pCommandList, depth->GetSRV(), normal->GetSRV(), velocity->GetSRV(), candidateRadiance->GetSRV(),
                candidateHitNormal->GetSRV(), candidateRay->GetSRV(), width, height);
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
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* outputReservoirRayDirection = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputReservoirRayDirection);
            RenderGraphTexture* outputReservoirSampleRadiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputReservoirSampleRadiance);
            RenderGraphTexture* outputReservoir = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputReservoir);
            SpatialResampling(pCommandList, depth->GetSRV(), normal->GetSRV(),
                outputReservoirRayDirection->GetUAV(), outputReservoirSampleRadiance->GetUAV(), outputReservoir->GetUAV(), width, height);
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
            RenderGraphTexture* reservoir = (RenderGraphTexture*)pRenderGraph->GetResource(data.reservoir);
            RenderGraphTexture* radiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.radiance);
            RenderGraphTexture* rayDirection = (RenderGraphTexture*)pRenderGraph->GetResource(data.rayDirection);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Resolve(pCommandList, reservoir->GetSRV(), radiance->GetSRV(), rayDirection->GetSRV(), normal->GetSRV(), output->GetUAV(), width, height);
        });

    return resolve_pass->output;
}

void ReSTIRGI::InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal,
    IGfxDescriptor* outputRadiance, IGfxDescriptor* outputHitNormal, IGfxDescriptor* outputRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pInitialSamplingPSO);

    uint32_t constants[5] = { 
        depth->GetHeapIndex(), 
        normal->GetHeapIndex(),
        outputRadiance->GetHeapIndex(),
        outputHitNormal->GetHeapIndex(),
        outputRay->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::TemporalResampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* velocity, 
    IGfxDescriptor* candidateRadiance, IGfxDescriptor* candidateHitNormal, IGfxDescriptor* candidateRay, uint32_t width, uint32_t height)
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
    cb.depth = depth->GetHeapIndex();
    cb.normal = normal->GetHeapIndex();
    cb.velocity = velocity->GetHeapIndex();
    cb.prevLinearDepth = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.prevNormal = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
    cb.candidateRadiance = candidateRadiance->GetHeapIndex();
    cb.candidateHitNormal = candidateHitNormal->GetHeapIndex();
    cb.candidateRay = candidateRay->GetHeapIndex();
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

void ReSTIRGI::SpatialResampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal,
    IGfxDescriptor* outputReservoirRayDirection, IGfxDescriptor* outputReservoirSampleRadiance, IGfxDescriptor* outputReservoir, uint32_t width, uint32_t height)
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
    constants.depth = depth->GetHeapIndex();
    constants.normal = normal->GetHeapIndex();
    constants.inputReservoirRayDirection = m_temporalReservoir[1].rayDirection->GetSRV()->GetHeapIndex();
    constants.inputReservoirSampleNormal = m_temporalReservoir[1].sampleNormal->GetSRV()->GetHeapIndex();
    constants.inputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetSRV()->GetHeapIndex();
    constants.inputReservoir = m_temporalReservoir[1].reservoir->GetSRV()->GetHeapIndex();
    constants.outputReservoirRayDirection = outputReservoirRayDirection->GetHeapIndex();
    constants.outputReservoirSampleRadiance = outputReservoirSampleRadiance->GetHeapIndex();
    constants.outputReservoir = outputReservoir->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::Resolve(IGfxCommandList* pCommandList, IGfxDescriptor* reservoir, IGfxDescriptor* radiance, IGfxDescriptor* rayDirection, IGfxDescriptor* normal,
    IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolvePSO);

    uint32_t constants[5] = { 
        reservoir->GetHeapIndex(),
        radiance->GetHeapIndex(),
        rayDirection->GetHeapIndex(),
        normal->GetHeapIndex(),
        output->GetHeapIndex() 
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
            m_temporalReservoir[i].rayDirection.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir rayDirection {}", i).c_str()));
            m_temporalReservoir[i].sampleNormal.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleNormal {}", i).c_str()));
            m_temporalReservoir[i].sampleRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleRadiance {}", i).c_str()));
            m_temporalReservoir[i].reservoir.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir {}", i).c_str()));
        }

        initialFrame = true;
    }

    return initialFrame;
}
