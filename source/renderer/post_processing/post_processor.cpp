#include "post_processor.h"
#include "taa.h"
#include "automatic_exposure.h"
#include "dof.h"
#include "motion_blur.h"
#include "bloom.h"
#include "tonemapper.h"
#include "fxaa.h"
#include "cas.h"
#include "fsr2.h"
#include "dlss.h"
#include "xess.h"
#include "metalfx_temporal_upscaler.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "utils/log.h"

PostProcessor::PostProcessor(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    m_pTAA = eastl::make_unique<TAA>(pRenderer);
    m_pAutomaticExposure = eastl::make_unique<AutomaticExposure>(pRenderer);
    m_pDoF = eastl::make_unique<DoF>(pRenderer);
    m_pMotionBlur = eastl::make_unique<MotionBlur>(pRenderer);
    m_pBloom = eastl::make_unique<Bloom>(pRenderer);
    m_pToneMapper = eastl::make_unique<Tonemapper>(pRenderer);
    m_pFXAA = eastl::make_unique<FXAA>(pRenderer);
    m_pCAS = eastl::make_unique<CAS>(pRenderer);
#if RE_PLATFORM_WINDOWS
    m_pFSR2 = eastl::make_unique<FSR2>(pRenderer);
    m_pDLSS = eastl::make_unique<DLSS>(pRenderer);
    m_pXeSS = eastl::make_unique<XeSS>(pRenderer);
#elif RE_PLATFORM_MAC || RE_PLATFORM_IOS
    m_pMetalFXTemporalUpscaler = eastl::make_unique<MetalFXTemporalUpscaler>(pRenderer);
#endif
}

PostProcessor::~PostProcessor() = default;

void PostProcessor::OnGui()
{
    World* world = Engine::GetInstance()->GetWorld();
    world->GetCamera()->OnPostProcessSettingGui();

    UpsacleModeGui();
#if RE_PLATFORM_WINDOWS
    m_pFSR2->OnGui();
    m_pDLSS->OnGui();
    m_pXeSS->OnGui();
#elif RE_PLATFORM_MAC || RE_PLATFORM_IOS
    m_pMetalFXTemporalUpscaler->OnGui();
#endif
    m_pTAA->OnGui();

    m_pDoF->OnGui();
    m_pMotionBlur->OnGui();

    m_pAutomaticExposure->OnGui();
    m_pBloom->OnGui();
    m_pToneMapper->OnGui();

    m_pFXAA->OnGui();
    m_pCAS->OnGui();
}

RGHandle PostProcessor::AddPass(RenderGraph* pRenderGraph, RGHandle sceneColorRT, RGHandle sceneDepthRT, RGHandle velocityRT,
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "PostProcess");

    bool needPostProcess = ShouldRenderPostProcess();

    RGHandle outputHandle = sceneColorRT;

    if (needPostProcess)
    {
        outputHandle = m_pDoF->AddPass(pRenderGraph, outputHandle, sceneDepthRT, renderWidth, renderHeight);
    }

    RGHandle exposure = m_pAutomaticExposure->AddPass(pRenderGraph, outputHandle, renderWidth, renderHeight);

    TemporalSuperResolution upscaleMode = m_pRenderer->GetTemporalUpscaleMode();
    switch (upscaleMode)
    {
#if RE_PLATFORM_WINDOWS
    case TemporalSuperResolution::FSR2:
        outputHandle = m_pFSR2->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, exposure, renderWidth, renderHeight, displayWidth, displayHeight);
        break;
    case TemporalSuperResolution::DLSS:
        outputHandle = m_pDLSS->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, exposure, renderWidth, renderHeight, displayWidth, displayHeight);
        break;
    case TemporalSuperResolution::XeSS:
        outputHandle = m_pXeSS->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, exposure, renderWidth, renderHeight, displayWidth, displayHeight);
        break;
#elif RE_PLATFORM_MAC || RE_PLATFORM_IOS
    case TemporalSuperResolution::MetalFX:
        outputHandle = m_pMetalFXTemporalUpscaler->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, exposure, renderWidth, renderHeight, displayWidth, displayHeight);
        break;
#endif
    default:
        RE_ASSERT(renderWidth == displayWidth && renderHeight == displayHeight);
        outputHandle = m_pTAA->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, displayWidth, displayHeight);
        break;
    }

    if (needPostProcess)
    {
        outputHandle = m_pMotionBlur->AddPass(pRenderGraph, outputHandle, sceneDepthRT, velocityRT, displayWidth, displayHeight);

        RGHandle bloom = m_pBloom->AddPass(pRenderGraph, outputHandle, displayWidth, displayHeight);
        outputHandle = m_pToneMapper->AddPass(pRenderGraph, outputHandle, exposure, bloom, m_pBloom->GetIntensity(), displayWidth, displayHeight);

        outputHandle = m_pFXAA->AddPass(pRenderGraph, outputHandle, displayWidth, displayHeight);

        if (upscaleMode != TemporalSuperResolution::FSR2) // FSR2 has a built-in RCAS
        {
            outputHandle = m_pCAS->AddPass(pRenderGraph, outputHandle, displayWidth, displayHeight);
        }
    }

    return outputHandle;
}

bool PostProcessor::RequiresCameraJitter()
{
    return m_pTAA->IsEnabled() || m_pRenderer->GetTemporalUpscaleMode() != TemporalSuperResolution::None;
}

void PostProcessor::UpsacleModeGui()
{
    TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
    
    if (ImGui::CollapsingHeader("Temporal Super Resolution"))
    {
        ImGui::Combo("Upscaler##TemporalUpscaler", (int*)&mode, "None\0FSR2\0DLSS\0XeSS\0MetalFX\0\0", (int)TemporalSuperResolution::Max);

#if RE_PLATFORM_WINDOWS
        if (mode == TemporalSuperResolution::DLSS && !m_pDLSS->IsSupported())
        {
            RE_INFO("DLSS is not supported on this device.");

            mode = TemporalSuperResolution::None;
        }
#endif
    }
    
    m_pRenderer->SetTemporalUpscaleMode(mode);

    switch (mode)
    {
#if RE_PLATFORM_WINDOWS
    case TemporalSuperResolution::FSR2:
        m_pRenderer->SetTemporalUpscaleRatio(m_pFSR2->GetUpscaleRatio());
        break;
    case TemporalSuperResolution::DLSS:
        m_pRenderer->SetTemporalUpscaleRatio(m_pDLSS->GetUpscaleRatio());
        break;
    case TemporalSuperResolution::XeSS:
        m_pRenderer->SetTemporalUpscaleRatio(m_pXeSS->GetUpscaleRatio());
        break;
#elif RE_PLATFORM_MAC || RE_PLATFORM_IOS
    case TemporalSuperResolution::MetalFX:
        m_pRenderer->SetTemporalUpscaleRatio(m_pMetalFXTemporalUpscaler->GetUpscaleRatio());
        break;
#endif
    default:
        m_pRenderer->SetTemporalUpscaleRatio(1.0f);
        break;
    }
}

bool PostProcessor::ShouldRenderPostProcess()
{
    return m_pRenderer->GetOutputType() == RendererOutput::Default ||
        m_pRenderer->GetOutputType() == RendererOutput::PathTracing ||
        m_pRenderer->GetOutputType() == RendererOutput::DirectLighting ||
        m_pRenderer->GetOutputType() == RendererOutput::IndirectDiffuse ||
        m_pRenderer->GetOutputType() == RendererOutput::IndirectSpecular;
}
