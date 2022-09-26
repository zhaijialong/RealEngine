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

RGHandle HybridStochasticReflection::Render(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Hybrid Stochastic Reflection",
        [&]()
        {
            if (ImGui::Checkbox("Enable##HSR", &m_bEnable))
            {
                m_pDenoiser->InvalidateHistory();
            }

            ImGui::Checkbox("Enable SW Ray##HSR", &m_bEnableSWRay);
            ImGui::Checkbox("Enable HW Ray##HSR", &m_bEnableHWRay);

            if (ImGui::Checkbox("Enable Denoiser##HSR", &m_bEnableDenoiser))
            {
                m_pDenoiser->InvalidateHistory();
            }

            ImGui::Text("Samples Per Quad : "); ImGui::SameLine();
            ImGui::RadioButton("1##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 1); ImGui::SameLine();
            ImGui::RadioButton("2##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 2); ImGui::SameLine();
            ImGui::RadioButton("4##HSR-samplesPerQuad", (int*)&m_samplesPerQuad, 4);

            ImGui::SliderFloat("Max Roughness##HSR", &m_maxRoughness, 0.0f, 1.0f);
            ImGui::SliderFloat("SSR Thickness##HSR", &m_ssrThickness, 0.0f, 1.0f);
            ImGui::SliderFloat("Temporal Stability##HSR", &m_temporalStability, 0.0f, 1.0f);
        });

    if (!m_bEnable)
    {
        return RGHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "HybridStochasticReflection");

    m_pDenoiser->ImportTextures(pRenderGraph, width, height);

    struct TileClassificationData
    {
        RGHandle depth;
        RGHandle normal;
        RGHandle historyVariance;
        RGHandle rayListBuffer;
        RGHandle tileListBuffer;
        RGHandle rayCounterBuffer;
    };

    RenderPassType pass_type = m_pRenderer->IsAsyncComputeEnabled() ? RenderPassType::AsyncCompute : RenderPassType::Compute;

    auto tile_classification_pass = pRenderGraph->AddPass<TileClassificationData>("HSR - tile classification", pass_type,
        [&](TileClassificationData& data, RGBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            if (m_bEnableDenoiser)
            {
                data.historyVariance = builder.Read(m_pDenoiser->GetHistoryVariance());
            }

            RGBuffer::Desc desc;
            desc.stride = sizeof(uint32_t);
            desc.size = sizeof(uint32_t) * 2;
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.rayCounterBuffer = builder.Create<RGBuffer>(desc, "HSR SW ray counter");
            data.rayCounterBuffer = builder.Write(data.rayCounterBuffer);

            desc.size = sizeof(uint32_t) * width * height;
            data.rayListBuffer = builder.Create<RGBuffer>(desc, "HSR SW ray list");
            data.rayListBuffer = builder.Write(data.rayListBuffer);

            uint32_t tile_count_x = DivideRoudingUp(width, 8);
            uint32_t tile_count_y = DivideRoudingUp(height, 8);
            desc.size = sizeof(uint32_t) * tile_count_x * tile_count_y;
            data.tileListBuffer = builder.Create<RGBuffer>(desc, "HSR tile list");
            data.tileListBuffer = builder.Write(data.tileListBuffer);
        },
        [=](const TileClassificationData& data, IGfxCommandList* pCommandList)
        {
            ClassifyTiles(pCommandList, 
                pRenderGraph->GetTexture(data.depth), 
                pRenderGraph->GetTexture(data.normal), 
                pRenderGraph->GetBuffer(data.rayListBuffer),
                pRenderGraph->GetBuffer(data.tileListBuffer),
                pRenderGraph->GetBuffer(data.rayCounterBuffer),
                width, height);
        });

    struct PrepareIndirectArgsData
    {
        RGHandle rayCounterBuffer;
        RGHandle indirectArgsBuffer;
        RGHandle denoiserArgsBuffer;
    };

    auto prepare_indirect_args_pass = pRenderGraph->AddPass<PrepareIndirectArgsData>("HSR - prepare indirect args", pass_type,
        [&](PrepareIndirectArgsData& data, RGBuilder& builder)
        {
            data.rayCounterBuffer = builder.Read(tile_classification_pass->rayCounterBuffer);

            RGBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.indirectArgsBuffer = builder.Create<RGBuffer>(desc, "HSR SSR indirect args");
            data.indirectArgsBuffer = builder.Write(data.indirectArgsBuffer);

            data.denoiserArgsBuffer = builder.Create<RGBuffer>(desc, "HSR denoiser indirect args");
            data.denoiserArgsBuffer = builder.Write(data.denoiserArgsBuffer);
        },
        [=](const PrepareIndirectArgsData& data, IGfxCommandList* pCommandList)
        {
            PrepareIndirectArgs(pCommandList, 
                pRenderGraph->GetBuffer(data.rayCounterBuffer),
                pRenderGraph->GetBuffer(data.indirectArgsBuffer),
                pRenderGraph->GetBuffer(data.denoiserArgsBuffer));
        });

    struct SSRData
    {
        RGHandle normal;
        RGHandle depth;
        RGHandle sceneHZB;
        RGHandle velocity;
        RGHandle prevSceneColor;
        RGHandle output;

        RGHandle rayCounterBufferSRV;
        RGHandle rayListBufferSRV;
        RGHandle indirectArgsBuffer;

        RGHandle hwRayCounterBufferUAV;
        RGHandle hwRayListBufferUAV;
    };

    auto ssr_pass = pRenderGraph->AddPass<SSRData>("HSR - SSR", pass_type,
        [&](SSRData& data, RGBuilder& builder)
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

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "HSR output");
            data.output = builder.Write(data.output);

            data.rayCounterBufferSRV = builder.Read(tile_classification_pass->rayCounterBuffer);
            data.rayListBufferSRV = builder.Read(tile_classification_pass->rayListBuffer);
            data.indirectArgsBuffer = builder.ReadIndirectArg(prepare_indirect_args_pass->indirectArgsBuffer);

            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint32_t);
            bufferDesc.size = sizeof(uint32_t);
            bufferDesc.format = GfxFormat::R32UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer;
            data.hwRayCounterBufferUAV = builder.Create<RGBuffer>(bufferDesc, "HSR HW ray counter");
            data.hwRayCounterBufferUAV = builder.Write(data.hwRayCounterBufferUAV);

            bufferDesc.size = sizeof(uint32_t) * width * height;
            data.hwRayListBufferUAV = builder.Create<RGBuffer>(bufferDesc, "HSR HW ray list");
            data.hwRayListBufferUAV = builder.Write(data.hwRayListBufferUAV);
        },
        [=](const SSRData& data, IGfxCommandList* pCommandList)
        {
            SSR(pCommandList, 
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.output),
                pRenderGraph->GetBuffer(data.rayCounterBufferSRV),
                pRenderGraph->GetBuffer(data.rayListBufferSRV),
                pRenderGraph->GetBuffer(data.indirectArgsBuffer),
                pRenderGraph->GetBuffer(data.hwRayCounterBufferUAV),
                pRenderGraph->GetBuffer(data.hwRayListBufferUAV));
        });

    struct PrepareRaytraceIndirectArgsData
    {
        RGHandle rayCounterBuffer;
        RGHandle indirectArgsBuffer;
    };

    auto prepare_rt_indirect_args_pass = pRenderGraph->AddPass<PrepareRaytraceIndirectArgsData>("HSR - prepare indirect args", pass_type,
        [&](PrepareRaytraceIndirectArgsData& data, RGBuilder& builder)
        {
            data.rayCounterBuffer = builder.Read(ssr_pass->hwRayCounterBufferUAV);

            RGBuffer::Desc desc;
            desc.size = sizeof(uint32_t) * 3;
            desc.stride = sizeof(uint32_t);
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxBufferUsageTypedBuffer;
            data.indirectArgsBuffer = builder.Create<RGBuffer>(desc, "HSR raytrace indirect args");
            data.indirectArgsBuffer = builder.Write(data.indirectArgsBuffer);
        },
        [=](const PrepareRaytraceIndirectArgsData& data, IGfxCommandList* pCommandList)
        {
            PrepareRaytraceIndirectArgs(pCommandList, 
                pRenderGraph->GetBuffer(data.rayCounterBuffer),
                pRenderGraph->GetBuffer(data.indirectArgsBuffer));
        });
    
    struct RaytraceData
    {
        RGHandle normal;
        RGHandle depth;
        RGHandle output;

        RGHandle rayCounterBufferSRV;
        RGHandle rayListBufferSRV;
        RGHandle indirectArgsBuffer;
    };

    bool isAMD = m_pRenderer->GetDevice()->GetVendor() == GfxVendor::AMD; //todo : AMD crashes with async compute here

    auto raytrace_pass = pRenderGraph->AddPass<RaytraceData>("HSR - raytrace", isAMD ? RenderPassType::Compute : pass_type,
        [&](RaytraceData& data, RGBuilder& builder)
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
            Raytrace(pCommandList, 
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.output),
                pRenderGraph->GetBuffer(data.rayCounterBufferSRV),
                pRenderGraph->GetBuffer(data.rayListBufferSRV),
                pRenderGraph->GetBuffer(data.indirectArgsBuffer));
        });

    if (m_bEnableDenoiser)
    {
        return m_pDenoiser->Render(pRenderGraph, prepare_indirect_args_pass->denoiserArgsBuffer, tile_classification_pass->tileListBuffer, raytrace_pass->output, 
            depth, linear_depth, normal, velocity, width, height, m_maxRoughness, m_temporalStability);
    }
    else
    {
        return raytrace_pass->output;
    }
}

void HybridStochasticReflection::ClassifyTiles(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* normal,
    RGBuffer* rayListUAV, RGBuffer* tileListUAV, RGBuffer* rayCounterUAV, uint32_t width, uint32_t height)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(rayCounterUAV->GetBuffer(), rayCounterUAV->GetUAV(), clear_value);
    pCommandList->UavBarrier(rayCounterUAV->GetBuffer());

    pCommandList->SetPipelineState(m_pTileClassificationPSO);
    SetRootConstants(pCommandList);

    bool enableVarianceGuidedTracing = m_bEnableDenoiser && m_pDenoiser->IsHistoryValid();

    uint32_t constants[8] = {
        depth->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        m_samplesPerQuad,
        enableVarianceGuidedTracing,
        m_pDenoiser->GetHistoryVarianceSRV()->GetHeapIndex(),
        rayListUAV->GetUAV()->GetHeapIndex(),
        tileListUAV->GetUAV()->GetHeapIndex(),
        rayCounterUAV->GetUAV()->GetHeapIndex(),
    };
    pCommandList->SetComputeConstants(1, constants, sizeof(constants));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void HybridStochasticReflection::PrepareIndirectArgs(IGfxCommandList* pCommandList, RGBuffer* rayCounterSRV, RGBuffer* indirectArgsUAV, RGBuffer* denoiserArgsUAV)
{
    pCommandList->SetPipelineState(m_pPrepareIndirectArgsPSO);

    uint32_t constants[3] = { rayCounterSRV->GetSRV()->GetHeapIndex(), indirectArgsUAV->GetUAV()->GetHeapIndex(), denoiserArgsUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(1, 1, 1);
}

void HybridStochasticReflection::SSR(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* depth, RGTexture* velocity, RGTexture* outputUAV,
    RGBuffer* rayCounter, RGBuffer* rayList, RGBuffer* indirectArgs, RGBuffer* hwRayCounterUAV, RGBuffer* hwRayListUAV)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(hwRayCounterUAV->GetBuffer(), hwRayCounterUAV->GetUAV(), clear_value);

    float clear_value1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCommandList->ClearUAV(outputUAV->GetTexture(), outputUAV->GetUAV(), clear_value1); //todo : this clear should not be needed

    pCommandList->UavBarrier(hwRayCounterUAV->GetBuffer());
    pCommandList->UavBarrier(outputUAV->GetTexture());

    pCommandList->SetPipelineState(m_pSSRPSO);
    SetRootConstants(pCommandList);

    uint constants[9] = {
        rayCounter->GetSRV()->GetHeapIndex(),
        rayList->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        depth->GetSRV()->GetHeapIndex(),
        velocity->GetSRV()->GetHeapIndex(),
        m_pRenderer->GetPrevSceneColorTexture()->GetSRV()->GetHeapIndex(),
        outputUAV->GetUAV()->GetHeapIndex(),
        hwRayCounterUAV->GetUAV()->GetHeapIndex(),
        hwRayListUAV->GetUAV()->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
}

void HybridStochasticReflection::PrepareRaytraceIndirectArgs(IGfxCommandList* pCommandList, RGBuffer* rayCounterSRV, RGBuffer* indirectArgsUAV)
{
    pCommandList->SetPipelineState(m_pPrepareRTArgsPSO);

    uint32_t constants[2] = { rayCounterSRV->GetSRV()->GetHeapIndex(), indirectArgsUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(1, 1, 1);
}

void HybridStochasticReflection::Raytrace(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* depth, RGTexture* outputUAV,
    RGBuffer* rayCounter, RGBuffer* rayList, RGBuffer* indirectArgs)
{
    pCommandList->SetPipelineState(m_pRaytracePSO);
    SetRootConstants(pCommandList);

    uint constants[5] = {
        rayCounter->GetSRV()->GetHeapIndex(),
        rayList->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        depth->GetSRV()->GetHeapIndex(),
        outputUAV->GetUAV()->GetHeapIndex(),
    };

    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
}

void HybridStochasticReflection::SetRootConstants(IGfxCommandList* pCommandList)
{
    struct RootConstants
    {
        float maxRoughness;
        float temporalStability;
        uint bEnableSWRay;
        float ssrThickness;
        uint bEnableHWRay;
    };

    RootConstants root_constants;
    root_constants.maxRoughness = m_maxRoughness;
    root_constants.temporalStability = m_temporalStability;
    root_constants.bEnableSWRay = m_bEnableSWRay;
    root_constants.ssrThickness = m_ssrThickness;
    root_constants.bEnableHWRay = m_bEnableHWRay;

    pCommandList->SetComputeConstants(0, &root_constants, sizeof(root_constants));
}
