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
        RenderGraphHandle outputIrradiance; // w : hitT
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
            data.outputIrradiance = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate irradiance"));

            desc.format = GfxFormat::RG16UNORM;
            data.outputHitNormal = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate hitNormal"));
            data.outputRay = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/candidate ray"));
        },
        [=](const RaytracePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* outputIrradiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputIrradiance);
            RenderGraphTexture* outputHitNormal = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputHitNormal);
            RenderGraphTexture* outputRay = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRay);
            InitialSampling(pCommandList, depth->GetSRV(), normal->GetSRV(), outputIrradiance->GetUAV(), outputHitNormal->GetUAV(), outputRay->GetUAV(), width, height);
        });

    if (!m_bEnableReSTIR)
    {
        return raytrace_pass->outputIrradiance;
    }

    bool initialFrame = InitTemporalBuffers(width, height);
    eastl::swap(m_temporalReservoir[0], m_temporalReservoir[1]);

    struct TemporalReusePassData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle velocity;

        RenderGraphHandle candidateIrradiance;
        RenderGraphHandle candidateHitNormal;
        RenderGraphHandle candidateRay;

        RenderGraphHandle historyReservoirPosition;
        RenderGraphHandle historyReservoirNormal;
        RenderGraphHandle historyReservoirRayDirection;
        RenderGraphHandle historyReservoirSampleNormal;
        RenderGraphHandle historyReservoirSampleRadiance;
        RenderGraphHandle historyReservoir;

        RenderGraphHandle outputReservoirPosition;
        RenderGraphHandle outputReservoirNormal;
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
            data.candidateIrradiance = builder.Read(raytrace_pass->outputIrradiance);
            data.candidateHitNormal = builder.Read(raytrace_pass->outputHitNormal);
            data.candidateRay = builder.Read(raytrace_pass->outputRay);

            GfxResourceState textureState = initialFrame ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS;

            data.historyReservoirPosition = builder.Read(builder.Import(m_temporalReservoir[0].position->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyReservoirNormal = builder.Read(builder.Import(m_temporalReservoir[0].normal->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyReservoirRayDirection = builder.Read(builder.Import(m_temporalReservoir[0].rayDirection->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyReservoirSampleNormal = builder.Read(builder.Import(m_temporalReservoir[0].sampleNormal->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyReservoirSampleRadiance = builder.Read(builder.Import(m_temporalReservoir[0].sampleRadiance->GetTexture(), textureState));
            data.historyReservoir = builder.Read(builder.Import(m_temporalReservoir[0].reservoir->GetTexture(), textureState));

            data.outputReservoirPosition = builder.Write(builder.Import(m_temporalReservoir[1].position->GetTexture(), textureState));
            data.outputReservoirNormal = builder.Write(builder.Import(m_temporalReservoir[1].normal->GetTexture(), textureState));
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
            RenderGraphTexture* candidateIrradiance = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateIrradiance);
            RenderGraphTexture* candidateHitNormal = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateHitNormal);
            RenderGraphTexture* candidateRay = (RenderGraphTexture*)pRenderGraph->GetResource(data.candidateRay);
            TemporalResampling(pCommandList, depth->GetSRV(), normal->GetSRV(), velocity->GetSRV(), candidateIrradiance->GetSRV(),
                candidateHitNormal->GetSRV(), candidateRay->GetSRV(), width, height);
        });


    struct ResolvePassData
    {
        RenderGraphHandle reservoir;
        RenderGraphHandle irradiance;
        RenderGraphHandle output;
    };

    auto resolve_pass = pRenderGraph->AddPass<ResolvePassData>("ReSTIR GI - resolve", RenderPassType::Compute,
        [&](ResolvePassData& data, RenderGraphBuilder& builder)
        {
            data.reservoir = builder.Read(temporal_reuse_pass->outputReservoir);
            data.irradiance = builder.Read(temporal_reuse_pass->outputReservoirSampleRadiance);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RenderGraphTexture>(desc, "ReSTIR GI/resolve output"));
        },
        [=](const ResolvePassData& data, IGfxCommandList* pCommandList)
        {
            IGfxDescriptor* reservoir = m_temporalReservoir[1].reservoir->GetSRV();
            IGfxDescriptor* irradiance = m_temporalReservoir[1].sampleRadiance->GetSRV();
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Resolve(pCommandList, reservoir, irradiance, output->GetUAV(), width, height);
        });

    return resolve_pass->output;
}

void ReSTIRGI::InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal,
    IGfxDescriptor* outputIrradiance, IGfxDescriptor* outputHitNormal, IGfxDescriptor* outputRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pInitialSamplingPSO);

    uint32_t constants[5] = { 
        depth->GetHeapIndex(), 
        normal->GetHeapIndex(),
        outputIrradiance->GetHeapIndex(),
        outputHitNormal->GetHeapIndex(),
        outputRay->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::TemporalResampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* velocity, 
    IGfxDescriptor* candidateIrradiance, IGfxDescriptor* candidateHitNormal, IGfxDescriptor* candidateRay, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalResamplingPSO);

    struct CB
    {
        uint depth;
        uint normal;
        uint velocity;
        uint candidateIrradiance;
        uint candidateHitNormal;
        uint candidateRay;
        uint historyReservoirPosition;
        uint historyReservoirNormal;
        uint historyReservoirRayDirection;
        uint historyReservoirSampleNormal;
        uint historyReservoirSampleRadiance;
        uint historyReservoir;
        uint outputReservoirPosition;
        uint outputReservoirNormal;
        uint outputReservoirRayDirection;
        uint outputReservoirSampleNormal;
        uint outputReservoirSampleRadiance;
        uint outputReservoir;
    };

    CB cb;
    cb.depth = depth->GetHeapIndex();
    cb.normal = normal->GetHeapIndex();
    cb.velocity = velocity->GetHeapIndex();
    cb.candidateIrradiance = candidateIrradiance->GetHeapIndex();
    cb.candidateHitNormal = candidateHitNormal->GetHeapIndex();
    cb.candidateRay = candidateRay->GetHeapIndex();
    cb.historyReservoirPosition = m_temporalReservoir[0].position->GetSRV()->GetHeapIndex();
    cb.historyReservoirNormal = m_temporalReservoir[0].normal->GetSRV()->GetHeapIndex();
    cb.historyReservoirRayDirection = m_temporalReservoir[0].rayDirection->GetSRV()->GetHeapIndex();
    cb.historyReservoirSampleNormal = m_temporalReservoir[0].sampleNormal->GetSRV()->GetHeapIndex();
    cb.historyReservoirSampleRadiance = m_temporalReservoir[0].sampleRadiance->GetSRV()->GetHeapIndex();
    cb.historyReservoir = m_temporalReservoir[0].reservoir->GetSRV()->GetHeapIndex();
    cb.outputReservoirPosition = m_temporalReservoir[1].position->GetUAV()->GetHeapIndex();
    cb.outputReservoirNormal = m_temporalReservoir[1].normal->GetUAV()->GetHeapIndex();
    cb.outputReservoirRayDirection = m_temporalReservoir[1].rayDirection->GetUAV()->GetHeapIndex();
    cb.outputReservoirSampleNormal = m_temporalReservoir[1].sampleNormal->GetUAV()->GetHeapIndex();
    cb.outputReservoirSampleRadiance = m_temporalReservoir[1].sampleRadiance->GetUAV()->GetHeapIndex();
    cb.outputReservoir = m_temporalReservoir[1].reservoir->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ReSTIRGI::Resolve(IGfxCommandList* pCommandList, IGfxDescriptor* reservoir, IGfxDescriptor* irradiance, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolvePSO);

    uint32_t constants[3] = { reservoir->GetHeapIndex(), irradiance->GetHeapIndex(), output->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

bool ReSTIRGI::InitTemporalBuffers(uint32_t width, uint32_t height)
{
    bool initialFrame = false;

    if (m_temporalReservoir[0].position == nullptr ||
        m_temporalReservoir[0].position->GetTexture()->GetDesc().width != width ||
        m_temporalReservoir[0].position->GetTexture()->GetDesc().height != height)
    {
        for (int i = 0; i < 2; ++i)
        {
            m_temporalReservoir[i].position.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, 
                fmt::format("ReSTIR GI/temporal reservoir poisition {}", i).c_str()));
            m_temporalReservoir[i].normal.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir normal {}", i).c_str()));
            m_temporalReservoir[i].rayDirection.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GItemporal reservoir rayDirection {}", i).c_str()));
            m_temporalReservoir[i].sampleNormal.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleNormal {}", i).c_str()));
            m_temporalReservoir[i].sampleRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir sampleRadiance {}", i).c_str()));
            m_temporalReservoir[i].reservoir.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess,
                fmt::format("ReSTIR GI/temporal reservoir {}", i).c_str()));
        }

        initialFrame = true;
    }

    return initialFrame;
}
