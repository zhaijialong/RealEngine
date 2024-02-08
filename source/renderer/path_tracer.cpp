#include "path_tracer.h"
#include "renderer.h"
#include "oidn.h"
#include "base_pass.h"
#include "core/engine.h"
#include "utils/gui_util.h"

PathTracer::PathTracer(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
#if WITH_OIDN
    m_denoiser = eastl::make_unique<OIDN>(pRenderer);
#endif

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("path_tracer.hlsl", "path_tracing", "cs_6_6", {});
    m_pPathTracingPSO = pRenderer->GetPipelineState(psoDesc, "PathTracing PSO");

    psoDesc.cs = pRenderer->GetShader("path_tracer.hlsl", "accumulation", "cs_6_6", {});
    m_pAccumulationPSO = pRenderer->GetPipelineState(psoDesc, "PathTracing accumulation PSO");
}

PathTracer::~PathTracer() = default;

RGHandle PathTracer::Render(RenderGraph* pRenderGraph, RGHandle depth, uint32_t width, uint32_t height)
{
    GUI("Lighting", "PathTracer", [&]()
        {
            m_bHistoryInvalid |= ImGui::Checkbox("Enable Accumulation##PathTracer", &m_bEnableAccumulation);
#if WITH_OIDN
            ImGui::Checkbox("Enable OIDN##PathTracer", &m_bEnableOIDN);
#endif
            m_bHistoryInvalid |= ImGui::SliderInt("Max Ray Length##PathTracer", (int*)&m_maxRayLength, 1, 16);
            m_bHistoryInvalid |= ImGui::SliderInt("Max Samples##PathTracer", (int*)&m_spp, 1, 8192);
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "PathTracer");

    if (m_pHistoryColor == nullptr ||
        m_pHistoryColor->GetTexture()->GetDesc().width != width ||
        m_pHistoryColor->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryColor.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, "PathTracer::m_pHistoryColor"));
        m_pHistoryAlbedo.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, "PathTracer::m_pHistoryAlbedo"));
        m_pHistoryNormal.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32F, GfxTextureUsageUnorderedAccess, "PathTracer::m_pHistoryNormal"));
        m_bHistoryInvalid = true;
    }

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    m_bHistoryInvalid |= camera->IsMoved();

    if (m_bHistoryInvalid)
    {
        m_currentSampleIndex = 0;
#if WITH_OIDN
        m_denoiser->Reset();
#endif
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
        RGHandle currentColor;
        RGHandle currentAlbedo;
        RGHandle currentNormal;
        RGHandle historyColor;
        RGHandle historyAlbedo;
        RGHandle historyNormal;
        RGHandle outputColor;
        RGHandle outputAlbedo;
        RGHandle outputNormal;
    };

    auto accumulation_pass = pRenderGraph->AddPass<AccumulationData>("Accumulation", RenderPassType::Compute,
        [&](AccumulationData& data, RGBuilder& builder)
        {
            if (tracingOutput.IsValid())
            {
                data.currentColor = builder.Read(tracingOutput);
            }

            BasePass* pBasePass = m_pRenderer->GetBassPass();
            data.currentAlbedo = builder.Read(pBasePass->GetDiffuseRT());
            data.currentNormal = builder.Read(pBasePass->GetNormalRT());

            data.historyColor = builder.Write(pRenderGraph->Import(m_pHistoryColor->GetTexture(), GfxAccessComputeUAV));
            data.historyAlbedo = builder.Write(pRenderGraph->Import(m_pHistoryAlbedo->GetTexture(), GfxAccessComputeUAV));
            data.historyNormal = builder.Write(pRenderGraph->Import(m_pHistoryNormal->GetTexture(), GfxAccessComputeUAV));

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.outputColor = builder.Write(builder.Create<RGTexture>(desc, "SceneColor RT"));
            data.outputAlbedo = builder.Write(builder.Create<RGTexture>(desc, "PathTracer albedo"));
            data.outputNormal = builder.Write(builder.Create<RGTexture>(desc, "PathTracer normal"));
        },
        [=](const AccumulationData& data, IGfxCommandList* pCommandList)
        {
            Accumulate(pCommandList, 
                pRenderGraph->GetTexture(data.currentColor), pRenderGraph->GetTexture(data.outputColor),
                pRenderGraph->GetTexture(data.currentAlbedo), pRenderGraph->GetTexture(data.outputAlbedo),
                pRenderGraph->GetTexture(data.currentNormal), pRenderGraph->GetTexture(data.outputNormal),
                width, height);
        });

#if WITH_OIDN
    if (m_bEnableOIDN && m_currentSampleIndex == m_spp)
    {
        struct OIDNPassData
        {
            RGHandle color;
            RGHandle albedo;
            RGHandle normal;
        };

        auto denoise_pass = pRenderGraph->AddPass<OIDNPassData>("OIDN", RenderPassType::Copy,
            [&](OIDNPassData& data, RGBuilder& builder)
            {
                data.color = builder.Write(accumulation_pass->outputColor);
                data.albedo = builder.Read(accumulation_pass->outputAlbedo);
                data.normal = builder.Read(accumulation_pass->outputNormal);

                builder.SkipCulling();
            },
            [=](const OIDNPassData& data, IGfxCommandList* pCommandList)
            {
                RGTexture* color = pRenderGraph->GetTexture(data.color);
                RGTexture* albedo = pRenderGraph->GetTexture(data.albedo);
                RGTexture* normal = pRenderGraph->GetTexture(data.normal);
                m_denoiser->Execute(pCommandList, color->GetTexture(), albedo->GetTexture(), normal->GetTexture());
            });

        return denoise_pass->color;
    }
#endif // WITH_OIDN

    return accumulation_pass->outputColor;
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

void PathTracer::Accumulate(IGfxCommandList* pCommandList, 
    RGTexture* inputColor, RGTexture* outputColor,
    RGTexture* inputAlbedo, RGTexture* outputAlbedo,
    RGTexture* inputNormal, RGTexture* outputNormal,
    uint32_t width, uint32_t height)
{
    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;

        float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pHistoryColor->GetTexture(), m_pHistoryColor->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pHistoryAlbedo->GetTexture(), m_pHistoryColor->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pHistoryNormal->GetTexture(), m_pHistoryColor->GetUAV(), clear_value);
        pCommandList->TextureBarrier(m_pHistoryColor->GetTexture(), 0, GfxAccessClearUAV, GfxAccessComputeUAV);
        pCommandList->TextureBarrier(m_pHistoryAlbedo->GetTexture(), 0, GfxAccessClearUAV, GfxAccessComputeUAV);
        pCommandList->TextureBarrier(m_pHistoryNormal->GetTexture(), 0, GfxAccessClearUAV, GfxAccessComputeUAV);
    }

    pCommandList->SetPipelineState(m_pAccumulationPSO);

    bool accumulationFinished = (m_currentSampleIndex >= m_spp);

    struct CB
    {
        uint currentColorTexture;
        uint currentAlbedoTexture;
        uint currentNormalTexture;
        uint historyColorTexture;
        uint historyAlbedoTexture;
        uint historyNormalTexture;
        uint outputColorTexture;
        uint outputAlebdoTexture;
        uint outputNormalTexture;
        uint accumulatedFrames;
        uint bAccumulationFinished;
    };

    CB cb;
    cb.currentColorTexture = inputColor ? inputColor->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    cb.currentAlbedoTexture = inputAlbedo->GetSRV()->GetHeapIndex();
    cb.currentNormalTexture = inputNormal->GetSRV()->GetHeapIndex();
    cb.historyColorTexture = m_pHistoryColor->GetUAV()->GetHeapIndex();
    cb.historyAlbedoTexture = m_pHistoryAlbedo->GetUAV()->GetHeapIndex();
    cb.historyNormalTexture = m_pHistoryNormal->GetUAV()->GetHeapIndex();
    cb.outputColorTexture = outputColor->GetUAV()->GetHeapIndex();
    cb.outputAlebdoTexture = outputAlbedo->GetUAV()->GetHeapIndex();
    cb.outputNormalTexture = outputNormal->GetUAV()->GetHeapIndex();
    cb.accumulatedFrames = m_currentSampleIndex;
    cb.bAccumulationFinished = accumulationFinished;

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
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
