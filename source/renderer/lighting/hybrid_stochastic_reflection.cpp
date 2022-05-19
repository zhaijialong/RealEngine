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

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/prepare_indirect_args.hlsl", "main", "cs_6_6", {});
    m_pPrepareIndirectArgsPSO = pRenderer->GetPipelineState(desc, "HSR prepare indirect args PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/ssr.hlsl", "main", "cs_6_6", {});
    m_pSSRPSO = pRenderer->GetPipelineState(desc, "SSR PSO");
}

RenderGraphHandle HybridStochasticReflection::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Hybrid Stochastic Reflection",
        [&]()
        {
            ImGui::Checkbox("Enable##HSR", &m_bEnable);
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
            data.rayCounterBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR ray counter");
            data.rayCounterBuffer = builder.Write(data.rayCounterBuffer);

            desc.size = sizeof(uint32_t) * width * height;
            data.rayListBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR ray list");
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
            data.indirectArgsBuffer = builder.Create<RenderGraphBuffer>(desc, "HSR indirect args");
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
            data.output = builder.Create<RenderGraphTexture>(desc, "SSR output");
            data.output = builder.Write(data.output);

            data.rayCounterBufferSRV = builder.Read(tile_classification_pass->rayCounterBuffer);
            data.rayListBufferSRV = builder.Read(tile_classification_pass->rayListBuffer);
            data.indirectArgsBuffer = builder.ReadIndirectArg(prepare_indirect_args_pass->indirectArgsBuffer);
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
            SSR(pCommandList, normal->GetSRV(), depth->GetSRV(), velocity->GetSRV(), output->GetUAV(), rayCounterBuffer->GetSRV(), rayListBuffer->GetSRV(), indirectArgsBuffer->GetBuffer());
        });
    
    if (m_bEnableDenoiser)
    {
        return m_pDenoiser->Render(pRenderGraph, prepare_indirect_args_pass->denoiserArgsBuffer, tile_classification_pass->tileListBuffer, ssr_pass->output, depth, linear_depth, normal, velocity, width, height);
    }
    else
    {
        return ssr_pass->output;
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
        historyVariance->GetHeapIndex(),
        rayListUAV->GetHeapIndex(),
        tileListUAV->GetHeapIndex(),
        rayCounterUAV->GetHeapIndex(),
        m_samplesPerQuad,
        m_bEnableDenoiser
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

void HybridStochasticReflection::SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs)
{
    pCommandList->SetPipelineState(m_pSSRPSO);
    SetRootConstants(pCommandList);

    uint constants[7] = {
        rayCounter->GetHeapIndex(),
        rayList->GetHeapIndex(),
        normal->GetHeapIndex(),
        depth->GetHeapIndex(),
        velocity->GetHeapIndex(),
        m_pRenderer->GetPrevSceneColorTexture()->GetSRV()->GetHeapIndex(),
        outputUAV->GetHeapIndex()
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
    };

    Rootconstants root_constants;
    root_constants.maxRoughness = m_maxRoughness;
    root_constants.temporalStability = m_temporalStability;

    pCommandList->SetComputeConstants(0, &root_constants, sizeof(root_constants));
}
