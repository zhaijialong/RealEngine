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

    TemporalSuperResolution upscaleMode = m_pRenderer->GetTemporalUpscaleMode();
    switch (upscaleMode)
    {
    case TemporalSuperResolution::FSR2:
        outputHandle = m_pFSR2->Render(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, exposure, displayWidth, displayHeight);
        break;
    default:
        RE_ASSERT(renderWidth == displayWidth && renderHeight == displayHeight);
        outputHandle = m_pTAA->Render(pRenderGraph, outputHandle, sceneDepthRT, linearDepthRT, velocityRT, displayWidth, displayHeight);
        break;
    }

    RenderGraphHandle bloom = m_pBloom->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);

    outputHandle = m_pToneMapper->Render(pRenderGraph, outputHandle, exposure, bloom, m_pBloom->GetIntensity(), displayWidth, displayHeight);
    outputHandle = m_pFXAA->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);

    if (upscaleMode == TemporalSuperResolution::None)
    {
        outputHandle = m_pCAS->Render(pRenderGraph, outputHandle, displayWidth, displayHeight);
    }

    return outputHandle;
}

bool PostProcessor::RequiresCameraJitter()
{
    return m_pTAA->IsEnabled() || m_pRenderer->GetTemporalUpscaleMode() != TemporalSuperResolution::None;
}

void PostProcessor::UpdateUpsacleMode()
{
    GUI("PostProcess", "Temporal Super Resolution", [&]()
        {
            TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
            ImGui::Combo("Upscaler##TemporalUpscaler", (int*)&mode, "None\0FSR 2.0\0\0", (int)TemporalSuperResolution::Max);
            m_pRenderer->SetTemporalUpscaleMode(mode);

            float ratio = m_pRenderer->GetTemporalUpscaleRatio();

            switch (mode)
            {
            case TemporalSuperResolution::FSR2:
            {
                static FfxFsr2QualityMode qualityMode = FFX_FSR2_QUALITY_MODE_QUALITY;
                ImGui::Combo("Mode##FSR2", (int*)&qualityMode, "Custom\0Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0\0", 5);
                
                if (qualityMode == 0)
                {
                    ImGui::SliderFloat("Upscale Ratio##TemporalUpscaler", &ratio, 1.0, 3.0);
                }
                else
                {
                    ratio = ffxFsr2GetUpscaleRatioFromQualityMode(qualityMode);
                }

                break;
            }
            default:
                ratio = 1.0f;
                break;
            }

            m_pRenderer->SetTemporalUpscaleRatio(ratio);
        });
}
