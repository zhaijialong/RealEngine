#include "hybrid_stochastic_reflection.h"
#include "../renderer.h"
#include "../hierarchical_depth_buffer.h"
#include "utils/gui_util.h"

HybridStochasticReflection::HybridStochasticReflection(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pDenoiser = eastl::make_unique<ReflectionDenoiser>(pRenderer);

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/classify_tiles.hlsl", "main", "cs_6_6", {});
    m_pTileClassificationPSO = pRenderer->GetPipelineState(desc, "HSR classify tiles PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/prepare_indirect_args.hlsl", "main", "cs_6_6", { "DENOISER_ARG=1"});
    m_pPrepareIndirectArgsPSO = pRenderer->GetPipelineState(desc, "HSR prepare indirect args PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/ssr.hlsl", "main", "cs_6_6", {});
    m_pSSRPSO = pRenderer->GetPipelineState(desc, "SSR PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/prepare_indirect_args.hlsl", "main", "cs_6_6", {});
    m_pPrepareRTArgsPSO = pRenderer->GetPipelineState(desc, "HSR prepare indirect args PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/ray_trace.hlsl", "main", "cs_6_6", {});
    m_pRaytracePSO = pRenderer->GetPipelineState(desc, "RT reflection PSO");
}

RenderGraphHandle HybridStochasticReflection::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Hybrid Stochastic Reflection",
        [&]()
        {
            ImGui::Checkbox("Enable##HSR", &m_bEnable);
            ImGui::Checkbox("Enable SW Ray##HSR", &m_bEnableSWRay);
            ImGui::Checkbox("Enable HW Ray##HSR", &m_bEnableHWRay);
            ImGui::Checkbox("Enable Denoiser##HSR", &m_bEnableDenoiser);

            ImGui::Text("Samples Per Quad : "); ImGui::SameLine();
            ImGui::RadioButton("1##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 1); ImGui::SameLine();
            ImGui::RadioButton("2##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 2); ImGui::SameLine();
            ImGui::RadioButton("4##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 4);

            ImGui::SliderFloat("Max Roughness##HSR", &m_maxRoughness, 0.0, 1.0);
            ImGui::SliderFloat("Temporal Stability##HSR", &m_temporalStability, 0.0, 1.0);
        });

    if (!m_bEnable)
    {
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "HybridStochasticReflection");

    m_pDenoiser->ImportTextures(pRenderGraph, width, height);

    struct TileClassificationData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle historyVariance;
        RenderGraphHandle rayListBuffer;
        RenderGraphHandle tileListBuffer;
        RenderGraphHandle rayCounterBuffer;
    };

    auto tile_classification_pass = pRenderGraph->AddPass<TileClassificationData>("HSR - tile classification", RenderPassType::Compute,
        [&](TileClassificationData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            if (m_bEnableDenoiser)
            {
                data.historyVariance = builder.Read(m_pDenoiser->GetHistoryVariance());
            }

            RenderGraphBuffer::Desc desc;
            desc.stride = sizeof(uint32_t);
            desc.size = sizeof(uint32_t) * 2;
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.rayCounterBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR SW ray counter");
            data.rayCounterBuffer = builder.Write(data.rayCounterBuffer);

            desc.size = sizeof(uint32_t) * width * height;
            data.rayListBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR SW ray list");
            data.rayListBuffer = builder.Write(data.rayListBuffer);

            uint32_t tile_count_x = (width + 7) / 8;
            uint32_t tile_count_y = (height + 7) / 8;
            desc.size = sizeof(uint32_t) * tile_count_x * tile_count_y;
            data.tileListBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR tile list");
            data.tileListBuffer = builder.Write(data.tileListBuffer);
        },
        [=](const TileClassificationData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphBuffer* rayListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayListBuffer);
            RenderGraphBuffer* tileListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileListBuffer);
            RenderGraphBuffer* rayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayCounterBuffer);

            uint32_t clear_value[4] = { 0, 0, 0, 0 };
            pCommandList->ClearUAV(rayCounterBuffer->GetBuffer(), rayCounterBuffer->GetUAV(), clear_value);
            pCommandList->UavBarrier(rayCounterBuffer->GetBuffer());

            ClassifyTiles(pCommandList, depth->GetSRV(), normal->GetSRV(), m_pDenoiser->GetHistoryVarianceSRV(), rayListBuffer->GetUAV(), tileListBuffer->GetUAV(), rayCounterBuffer->GetUAV(), width, height);
        });

    struct PrepareIndirectArgsData
    {
        RenderGraphHandle rayCounterBuffer;
        RenderGraphHandle indirectArgsBuffer;
        RenderGraphHandle denoiserArgsBuffer;
    };

    auto prepare_indirect_args_pass = pRenderGraph->AddPass<PrepareIndirectArgsData>("HSR - prepare indirect args", RenderPassType::Compute,
        [&](PrepareIndirectArgsData& data, RenderGraphBuilder& builder)
        {
            data.rayCounterBuffer = builder.Read(tile_classification_pass->rayCounterBuffer);

            RenderGraphBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.indirectArgsBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR SSR indirect args");
            data.indirectArgsBuffer = builder.Write(data.indirectArgsBuffer);

            data.denoiserArgsBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR denoiser indirect args");
            data.denoiserArgsBuffer = builder.Write(data.denoiserArgsBuffer);
        },
        [=](const PrepareIndirectArgsData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* rayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayCounterBuffer);
            RenderGraphBuffer* indirectArgsBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgsBuffer);
            RenderGraphBuffer* denoiserArgsBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.denoiserArgsBuffer);
            PrepareIndirectArgs(pCommandList, rayCounterBuffer->GetSRV(), indirectArgsBuffer->GetUAV(), denoiserArgsBuffer->GetUAV());
        });

    struct SSRData
    {
        RenderGraphHandle normal;
        RenderGraphHandle depth;
        RenderGraphHandle sceneHZB;
        RenderGraphHandle velocity;
        RenderGraphHandle prevSceneColor;
        RenderGraphHandle output;

        RenderGraphHandle rayCounterBufferSRV;
        RenderGraphHandle rayListBufferSRV;
        RenderGraphHandle indirectArgsBuffer;

        RenderGraphHandle hwRayCounterBufferUAV;
        RenderGraphHandle hwRayListBufferUAV;
    };

    auto ssr_pass = pRenderGraph->AddPass<SSRData>("HSR - SSR", RenderPassType::Compute,
        [&](SSRData& data, RenderGraphBuilder& builder)
        {
            data.normal = builder.Read(normal);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.prevSceneColor = builder.Read(m_pRenderer->GetPrevSceneColorHandle());

            HZB* pHZB = m_pRenderer->GetHZB();
            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.sceneHZB = builder.Read(pHZB->GetSceneHZBMip(i), i);
            }

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "HSR output");
            data.output = builder.Write(data.output);

            data.rayCounterBufferSRV = builder.Read(tile_classification_pass->rayCounterBuffer);
            data.rayListBufferSRV = builder.Read(tile_classification_pass->rayListBuffer);
            data.indirectArgsBuffer = builder.ReadIndirectArg(prepare_indirect_args_pass->indirectArgsBuffer);

            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint32_t);
            bufferDesc.size = sizeof(uint32_t);
            bufferDesc.format = GfxFormat::R32UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer;
            data.hwRayCounterBufferUAV = builder.Create<RenderGraphBuffer>(bufferDesc, "HSR HW ray counter");
            data.hwRayCounterBufferUAV = builder.Write(data.hwRayCounterBufferUAV);

            bufferDesc.size = sizeof(uint32_t) * width * height;
            data.hwRayListBufferUAV = builder.Create<RenderGraphBuffer>(bufferDesc, "HSR HW ray list");
            data.hwRayListBufferUAV = builder.Write(data.hwRayListBufferUAV);
        },
        [=](const SSRData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* velocity = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            RenderGraphBuffer* rayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayCounterBufferSRV);
            RenderGraphBuffer* rayListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayListBufferSRV);
            RenderGraphBuffer* indirectArgsBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgsBuffer);
            RenderGraphBuffer* hwRayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.hwRayCounterBufferUAV);
            RenderGraphBuffer* hwRayListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.hwRayListBufferUAV);

            uint32_t clear_value[4] = { 0, 0, 0, 0 };
            pCommandList->ClearUAV(hwRayCounterBuffer->GetBuffer(), hwRayCounterBuffer->GetUAV(), clear_value);
            pCommandList->UavBarrier(hwRayCounterBuffer->GetBuffer());

            SSR(pCommandList, normal->GetSRV(), depth->GetSRV(), velocity->GetSRV(), output->GetUAV(), 
                rayCounterBuffer->GetSRV(), rayListBuffer->GetSRV(), indirectArgsBuffer->GetBuffer(), hwRayCounterBuffer->GetUAV(), hwRayListBuffer->GetUAV());
        });

    struct PrepareRaytraceIndirectArgsData
    {
        RenderGraphHandle rayCounterBuffer;
        RenderGraphHandle indirectArgsBuffer;
    };

    auto prepare_rt_indirect_args_pass = pRenderGraph->AddPass<PrepareRaytraceIndirectArgsData>("HSR - prepare indirect args", RenderPassType::Compute,
        [&](PrepareRaytraceIndirectArgsData& data, RenderGraphBuilder& builder)
        {
            data.rayCounterBuffer = builder.Read(ssr_pass->hwRayCounterBufferUAV);

            RenderGraphBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.indirectArgsBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR raytrace indirect args");
            data.indirectArgsBuffer = builder.Write(data.indirectArgsBuffer);
        },
        [=](const PrepareRaytraceIndirectArgsData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* rayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayCounterBuffer);
            RenderGraphBuffer* indirectArgsBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgsBuffer);
            PrepareRaytraceIndirectArgs(pCommandList, rayCounterBuffer->GetSRV(), indirectArgsBuffer->GetUAV());
        });
    
    struct RaytraceData
    {
        RenderGraphHandle normal;
        RenderGraphHandle depth;
        RenderGraphHandle output;

        RenderGraphHandle rayCounterBufferSRV;
        RenderGraphHandle rayListBufferSRV;
        RenderGraphHandle indirectArgsBuffer;
    };

    auto raytrace_pass = pRenderGraph->AddPass<RaytraceData>("HSR - raytrace", RenderPassType::Compute,
        [&](RaytraceData& data, RenderGraphBuilder& builder)
        {
            data.normal = builder.Read(normal);
            data.depth = builder.Read(depth);
            data.output = builder.Write(ssr_pass->output);
            data.rayCounterBufferSRV = builder.Read(prepare_rt_indirect_args_pass->rayCounterBuffer);
            data.rayListBufferSRV = builder.Read(ssr_pass->hwRayListBufferUAV);
            data.indirectArgsBuffer = builder.ReadIndirectArg(prepare_rt_indirect_args_pass->indirectArgsBuffer);
        },
        [=](const RaytraceData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            RenderGraphBuffer* rayCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayCounterBufferSRV);
            RenderGraphBuffer* rayListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.rayListBufferSRV);
            RenderGraphBuffer* indirectArgsBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgsBuffer);
            Raytrace(pCommandList, normal->GetSRV(), depth->GetSRV(), output->GetUAV(), rayCounterBuffer->GetSRV(), rayListBuffer->GetSRV(), indirectArgsBuffer->GetBuffer());
        });

    if (m_bEnableDenoiser)
    {
        return m_pDenoiser->Render(pRenderGraph, prepare_indirect_args_pass->denoiserArgsBuffer, tile_classification_pass->tileListBuffer, raytrace_pass->output, depth, linear_depth, normal, velocity, width, height);
    }
    else
    {
        return raytrace_pass->output;
    }
}

void HybridStochasticReflection::ClassifyTiles(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* historyVariance,
    IGfxDescriptor* rayListUAV, IGfxDescriptor* tileListUAV, IGfxDescriptor* rayCounterUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTileClassificationPSO);
    SetRootConstants(pCommandList);

    uint32_t constants[8] = {
        depth->GetHeapIndex(),
        normal->GetHeapIndex(),
        m_samplesPerQuad,
        m_bEnableDenoiser,
        historyVariance->GetHeapIndex(),
        rayListUAV->GetHeapIndex(),
        tileListUAV->GetHeapIndex(),
        rayCounterUAV->GetHeapIndex(),
    };
    pCommandList->SetComputeConstants(1, constants, sizeof(constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void HybridStochasticReflection::PrepareIndirectArgs(IGfxCommandList* pCommandList, IGfxDescriptor* rayCounterSRV, IGfxDescriptor* indirectArgsUAV, IGfxDescriptor* denoiserArgsUAV)
{
    pCommandList->SetPipelineState(m_pPrepareIndirectArgsPSO);

    uint32_t constants[3] = { rayCounterSRV->GetHeapIndex(), indirectArgsUAV->GetHeapIndex(), denoiserArgsUAV->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(1, 1, 1);
}

void HybridStochasticReflection::SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, 
    IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs, IGfxDescriptor* hwRayCounterUAV, IGfxDescriptor* hwRayListUAV)
{
    pCommandList->SetPipelineState(m_pSSRPSO);
    SetRootConstants(pCommandList);

    uint constants[9] = {
        rayCounter->GetHeapIndex(),
        rayList->GetHeapIndex(),
        normal->GetHeapIndex(),
        depth->GetHeapIndex(),
        velocity->GetHeapIndex(),
        m_pRenderer->GetPrevSceneColorTexture()->GetSRV()->GetHeapIndex(),
        outputUAV->GetHeapIndex(),
        hwRayCounterUAV->GetHeapIndex(),
        hwRayListUAV->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs, 0);
}

void HybridStochasticReflection::PrepareRaytraceIndirectArgs(IGfxCommandList* pCommandList, IGfxDescriptor* rayCounterSRV, IGfxDescriptor* indirectArgsUAV)
{
    pCommandList->SetPipelineState(m_pPrepareRTArgsPSO);

    uint32_t constants[2] = { rayCounterSRV->GetHeapIndex(), indirectArgsUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(1, 1, 1);
}

void HybridStochasticReflection::Raytrace(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* outputUAV, IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs)
{
    pCommandList->SetPipelineState(m_pRaytracePSO);
    SetRootConstants(pCommandList);

    uint constants[5] = {
        rayCounter->GetHeapIndex(),
        rayList->GetHeapIndex(),
        normal->GetHeapIndex(),
        depth->GetHeapIndex(),
        outputUAV->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs, 0);
}

void HybridStochasticReflection::SetRootConstants(IGfxCommandList* pCommandList)
{
    struct Rootconstants
    {
        float maxRoughness;
        float temporalStability;
        uint bEnableSWRay;
        uint bEnableHWRay;
    };

    Rootconstants root_constants;
    root_constants.maxRoughness = m_maxRoughness;
    root_constants.temporalStability = m_temporalStability;
    root_constants.bEnableSWRay = m_bEnableSWRay;
    root_constants.bEnableHWRay = m_bEnableHWRay;

    pCommandList->SetComputeConstants(0, &root_constants, sizeof(root_constants));
}
