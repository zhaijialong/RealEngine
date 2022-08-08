#include "path_tracer.h"
#include "renderer.h"
#include "base_pass.h"
#include "core/engine.h"
#include "utils/gui_util.h"

PathTracer::PathTracer(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("path_tracer.hlsl", "path_tracing", "cs_6_6", {});
    m_pPathTracingPSO = pRenderer->GetPipelineState(psoDesc, "PathTracing PSO");

    psoDesc.cs = pRenderer->GetShader("path_tracer.hlsl", "accumulation", "cs_6_6", {});
    m_pAccumulationPSO = pRenderer->GetPipelineState(psoDesc, "PathTracing accumulation PSO");
}

RenderGraphHandle PathTracer::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, uint32_t width, uint32_t height)
{
    GUI("Settings", "PathTracer", [&]()
        {
            if (ImGui::Checkbox("Enable Accumulation##PathTracer", &m_bEnableAccumulation))
            {
                m_bHistoryInvalid = true;
            }
            ImGui::SliderInt("Max Ray Length##PathTracer", (int*)&m_maxRayLength, 1, 32);
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "PathTracer");

    struct PathTracingData
    {
        RenderGraphHandle diffuseRT;
        RenderGraphHandle specularRT;
        RenderGraphHandle normalRT;
        RenderGraphHandle emissiveRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle output;
    };

    auto pt_pass = pRenderGraph->AddPass<PathTracingData>("PathTracing", RenderPassType::Compute,
        [&](PathTracingData& data, RenderGraphBuilder& builder)
        {
            BasePass* pBasePass = m_pRenderer->GetBassPass();
            
            data.diffuseRT = builder.Read(pBasePass->GetDiffuseRT());
            data.specularRT = builder.Read(pBasePass->GetSpecularRT());
            data.normalRT = builder.Read(pBasePass->GetNormalRT());
            data.emissiveRT = builder.Read(pBasePass->GetEmissiveRT());
            data.depthRT = builder.Read(depth);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32F;
            data.output = builder.Create<RenderGraphTexture>(desc, "PathTracing output");
            data.output = builder.Write(data.output);
        },
        [=](const PathTracingData& data, IGfxCommandList* pCommandList)
        {
            PathTrace(pCommandList,
                pRenderGraph->GetTexture(data.diffuseRT), 
                pRenderGraph->GetTexture(data.specularRT),
                pRenderGraph->GetTexture(data.normalRT),
                pRenderGraph->GetTexture(data.emissiveRT),
                pRenderGraph->GetTexture(data.depthRT),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    struct AccumulationData
    {
        RenderGraphHandle currentFrame;
        RenderGraphHandle history;
        RenderGraphHandle output;
    };

    if (m_pHistoryAccumulation == nullptr ||
        m_pHistoryAccumulation->GetTexture()->GetDesc().width != width ||
        m_pHistoryAccumulation->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryAccumulation.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, "PathTracer::m_pHistoryAccumulation"));
        m_bHistoryInvalid = true;
    }

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    m_bHistoryInvalid = camera->IsMoved() ? true : false;

    RenderGraphHandle history = pRenderGraph->Import(m_pHistoryAccumulation->GetTexture(), GfxResourceState::UnorderedAccess);

    auto accumulation_pass = pRenderGraph->AddPass<AccumulationData>("Accumulation", RenderPassType::Compute,
        [&](AccumulationData& data, RenderGraphBuilder& builder)
        {
            data.currentFrame = builder.Read(pt_pass->output);
            data.history = builder.Write(history);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "SceneColor RT");
            data.output = builder.Write(data.output);
        },
        [=](const AccumulationData& data, IGfxCommandList* pCommandList)
        {
            Accumulate(pCommandList, 
                pRenderGraph->GetTexture(data.currentFrame),
                pRenderGraph->GetTexture(data.output), 
                width, height);
        });

    return accumulation_pass->output;
}

void PathTracer::PathTrace(IGfxCommandList* pCommandList, RenderGraphTexture* diffuse, RenderGraphTexture* specular, RenderGraphTexture* normal, RenderGraphTexture* emissive, RenderGraphTexture* depth,
    RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPathTracingPSO);

    uint32_t constants[7] = {
        diffuse->GetSRV()->GetHeapIndex(),
        specular->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        emissive->GetSRV()->GetHeapIndex(),
        depth->GetSRV()->GetHeapIndex(),
        m_maxRayLength,
        output->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void PathTracer::Accumulate(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* outputUAV, uint32_t width, uint32_t height)
{
    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;
        m_nAccumulatedFrames = 0;

        float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pHistoryAccumulation->GetTexture(), m_pHistoryAccumulation->GetUAV(), clear_value);
        pCommandList->UavBarrier(m_pHistoryAccumulation->GetTexture());
    }

    pCommandList->SetPipelineState(m_pAccumulationPSO);

    uint32_t constants[5] = { 
        input->GetSRV()->GetHeapIndex(),
        m_pHistoryAccumulation->GetUAV()->GetHeapIndex(), 
        outputUAV->GetUAV()->GetHeapIndex(), 
        m_nAccumulatedFrames++, 
        m_bEnableAccumulation
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
