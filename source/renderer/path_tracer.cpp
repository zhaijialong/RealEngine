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

RenderGraphHandle PathTracer::Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
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

    auto pt_pass = pRenderGraph->AddPass<PathTracingData>("PathTracing",
        [&](PathTracingData& data, RenderGraphBuilder& builder)
        {
            BasePass* pBasePass = m_pRenderer->GetBassPass();
            
            data.diffuseRT = builder.Read(pBasePass->GetDiffuseRT(), GfxResourceState::ShaderResourceNonPS);
            data.specularRT = builder.Read(pBasePass->GetSpecularRT(), GfxResourceState::ShaderResourceNonPS);
            data.normalRT = builder.Read(pBasePass->GetNormalRT(), GfxResourceState::ShaderResourceNonPS);
            data.emissiveRT = builder.Read(pBasePass->GetEmissiveRT(), GfxResourceState::ShaderResourceNonPS);
            data.depthRT = builder.Read(pBasePass->GetDepthRT(), GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.output = builder.Create<RenderGraphTexture>(desc, "PathTracing output");
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const PathTracingData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* diffuseRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.diffuseRT);
            RenderGraphTexture* specularRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.specularRT);
            RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalRT);
            RenderGraphTexture* emissiveRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.emissiveRT);
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthRT);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            PathTrace(pCommandList, diffuseRT->GetSRV(), specularRT->GetSRV(), normalRT->GetSRV(), emissiveRT->GetSRV(), depthRT->GetSRV(), output->GetUAV(), width, height);
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

    auto accumulation_pass = pRenderGraph->AddPass<AccumulationData>("Accumulation",
        [&](AccumulationData& data, RenderGraphBuilder& builder)
        {
            data.currentFrame = builder.Read(pt_pass->output, GfxResourceState::ShaderResourceNonPS);
            data.history = builder.Write(history, GfxResourceState::UnorderedAccess);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageRenderTarget;
            data.output = builder.Create<RenderGraphTexture>(desc, "SceneColor RT");
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const AccumulationData& data, IGfxCommandList* pCommandList)
        {
            if (m_bHistoryInvalid)
            {
                m_bHistoryInvalid = false;
                m_nAccumulatedFrames = 0;

                float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                pCommandList->ClearUAV(m_pHistoryAccumulation->GetTexture(), m_pHistoryAccumulation->GetUAV(), clear_value);
            }

            RenderGraphTexture* currentFrame = (RenderGraphTexture*)pRenderGraph->GetResource(data.currentFrame);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Accumulate(pCommandList, currentFrame->GetSRV(), m_pHistoryAccumulation->GetUAV(), output->GetUAV(), width, height);
        });

    return accumulation_pass->output;
}

void PathTracer::PathTrace(IGfxCommandList* pCommandList, IGfxDescriptor* diffuse, IGfxDescriptor* specular, IGfxDescriptor* normal, IGfxDescriptor* emissive, IGfxDescriptor* depth,
    IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPathTracingPSO);

    uint32_t constants[7] = {
        diffuse->GetHeapIndex(),
        specular->GetHeapIndex(),
        normal->GetHeapIndex(),
        emissive->GetHeapIndex(),
        depth->GetHeapIndex(),
        m_maxRayLength,
        output->GetHeapIndex() 
    };
    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void PathTracer::Accumulate(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* historyUAV, IGfxDescriptor* outputUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pAccumulationPSO);

    uint32_t constants[5] = { input->GetHeapIndex(), historyUAV->GetHeapIndex(), outputUAV->GetHeapIndex(), m_nAccumulatedFrames++, m_bEnableAccumulation };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
