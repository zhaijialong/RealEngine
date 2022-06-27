#include "post_processor.h"
#include "../renderer.h"
#include "utils/gui_util.h"

PostProcessor::PostProcessor(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    m_pTAA = eastl::make_unique<TAA>(pRenderer);
    m_pAutomaticExposure = eastl::make_unique<AutomaticExposure>(pRenderer);
    m_pBloom = eastl::make_unique<Bloom>(pRenderer);
    m_pToneMapper = eastl::make_unique<Tonemapper>(pRenderer);
    m_pFXAA = eastl::make_unique<FXAA>(pRenderer);
    m_pCAS = eastl::make_unique<CAS>(pRenderer);
    m_pFSR2 = eastl::make_unique<FSR2>(pRenderer);
}

RenderGraphHandle PostProcessor::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, RenderGraphHandle sceneDepthRT,
    RenderGraphHandle linearDepthRT, RenderGraphHandle velocityRT, 
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "PostProcess");

    UpdateUpsacleMode();

    RenderGraphHandle outputHandle = sceneColorRT;
    RenderGraphHandle exposure = m_pAutomaticExposure->Render(pRenderGraph, outputHandle, renderWidth, renderHeight);

    switch (m_pRenderer->GetTemporalUpscaleMode())
    {
    case TemporalSuperResolution::FSR2:
        break;
    case TemporalSuperResolution::None:
    default:
        RE_ASSERT(renderWidth == displayWidth && renderHeight == displayHeight);
        outputHandle = m_pTAA->Render(pRenderGraph, outputHandle, sceneDepthRT, linearDepthRT, velocityRT, displayWidth, displayHeight);
        break;
    }

    RenderGraphHandle bloom = m_pBloom->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);

    outputHandle = m_pToneMapper->Render(pRenderGraph, outputHandle, exposure, bloom, m_pBloom->GetIntensity(), displayWidth, displayHeight);
    outputHandle = m_pFXAA->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);
    outputHandle = m_pCAS->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);

    return outputHandle;
}

bool PostProcessor::RequiresCameraJitter()
{
    return m_pTAA->IsEnabled();
}

void PostProcessor::UpdateUpsacleMode()
{
    GUI("Settings", "Temporal Super Resolution", [&]()
        {
            TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
            float ratio = m_pRenderer->GetTemporalUpscaleRatio();

            ImGui::Combo("Mode##TemporalUpscaler", (int*)&mode, "None\0FSR 2.0\0\0", (int)TemporalSuperResolution::Max);
            ImGui::SliderFloat("Upscale Ratio##TemporalUpscaler", &ratio, 0.5, 1.0);

            m_pRenderer->SetTemporalUpscaleMode(mode);
            m_pRenderer->SetTemporalUpscaleRatio(ratio);
        });
}
