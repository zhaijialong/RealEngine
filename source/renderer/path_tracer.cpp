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

RGHandle PathTracer::Render(RenderGraph* pRenderGraph, RGHandle depth, uint32_t width, uint32_t height)
{
    GUI("Lighting", "PathTracer", [&]()
        {
            m_bHistoryInvalid |= ImGui::Checkbox("Enable Accumulation##PathTracer", &m_bEnableAccumulation);
            m_bHistoryInvalid |= ImGui::SliderInt("Max Ray Length##PathTracer", (int*)&m_maxRayLength, 1, 16);
            m_bHistoryInvalid |= ImGui::SliderInt("Max Samples##PathTracer", (int*)&m_spp, 1, 8192);
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "PathTracer");

    if (m_pHistoryAccumulation == nullptr ||
        m_pHistoryAccumulation->GetTexture()->GetDesc().width != width ||
        m_pHistoryAccumulation->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryAccumulation.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, "PathTracer::m_pHistoryAccumulation"));
        m_bHistoryInvalid = true;
    }

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    m_bHistoryInvalid |= camera->IsMoved();

    if (m_bHistoryInvalid)
    {
        m_currentSampleIndex = 0;
    }

    RGHandle tracingOutput;

    if (m_currentSampleIndex < m_spp)
    {
        struct PathTracingData
        {
            RGHandle diffuseRT;
            RGHandle specularRT;
            RGHandle normalRT;
            RGHandle emissiveRT;
            RGHandle depthRT;
            RGHandle output;
        };

        auto pt_pass = pRenderGraph->AddPass<PathTracingData>("PathTracing", RenderPassType::Compute,
            [&](PathTracingData& data, RGBuilder& builder)
            {
                BasePass* pBasePass = m_pRenderer->GetBassPass();

                data.diffuseRT = builder.Read(pBasePass->GetDiffuseRT());
                data.specularRT = builder.Read(pBasePass->GetSpecularRT());
                data.normalRT = builder.Read(pBasePass->GetNormalRT());
                data.emissiveRT = builder.Read(pBasePass->GetEmissiveRT());
                data.depthRT = builder.Read(depth);

                RGTexture::Desc desc;
                desc.width = width;
                desc.height = height;
                desc.format = GfxFormat::RGBA16F;
                data.output = builder.Create<RGTexture>(desc, "PathTracing output");
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

        tracingOutput = pt_pass->output;
    }

    if (!m_bEnableAccumulation)
    {
        return tracingOutput;
    }

    RenderProgressBar();

    struct AccumulationData
    {
        RGHandle currentFrame;
        RGHandle history;
        RGHandle output;
    };

    auto accumulation_pass = pRenderGraph->AddPass<AccumulationData>("Accumulation", RenderPassType::Compute,
        [&](AccumulationData& data, RGBuilder& builder)
        {
            if (tracingOutput.IsValid())
            {
                data.currentFrame = builder.Read(tracingOutput);
            }
            data.history = builder.Write(pRenderGraph->Import(m_pHistoryAccumulation->GetTexture(), GfxAccessComputeUAV));

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "SceneColor RT");
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

void PathTracer::PathTrace(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal, RGTexture* emissive, RGTexture* depth,
    RGTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPathTracingPSO);

    uint32_t constants[9] = {
        diffuse->GetSRV()->GetHeapIndex(),
        specular->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        emissive->GetSRV()->GetHeapIndex(),
        depth->GetSRV()->GetHeapIndex(),
        m_maxRayLength,
        m_currentSampleIndex,
        m_spp,
        output->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(1, constants, sizeof(constants));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void PathTracer::Accumulate(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* outputUAV, uint32_t width, uint32_t height)
{
    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;

        float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pHistoryAccumulation->GetTexture(), m_pHistoryAccumulation->GetUAV(), clear_value);
        pCommandList->TextureBarrier(m_pHistoryAccumulation->GetTexture(), 0, GfxAccessClearUAV, GfxAccessComputeUAV);
    }

    pCommandList->SetPipelineState(m_pAccumulationPSO);

    bool accumulationFinished = (m_currentSampleIndex >= m_spp);

    uint32_t constants[5] = { 
        input ? input->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
        m_pHistoryAccumulation->GetUAV()->GetHeapIndex(), 
        outputUAV->GetUAV()->GetHeapIndex(), 
        m_currentSampleIndex,
        accumulationFinished
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);

    if (!accumulationFinished)
    {
        ++m_currentSampleIndex;
    }
}

void PathTracer::RenderProgressBar()
{
    ImGuiIO& io = ImGui::GetIO();
    const uint32_t progressBarSize = 550;

    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - progressBarSize) / 2 + 50, io.DisplaySize.y - 70));
    ImGui::SetNextWindowSize(ImVec2(progressBarSize, 30));

    ImGui::Begin("PathtracingProgress", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

    ImGui::ProgressBar((float)m_currentSampleIndex / m_spp, ImVec2(450, 0));
    ImGui::SameLine();
    ImGui::Text("%d/%d", m_currentSampleIndex, m_spp);

    ImGui::End();
}
