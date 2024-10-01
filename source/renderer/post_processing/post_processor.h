#pragma once

#include "../render_graph.h"
#include "core/platform.h"

class PostProcessor
{
public:
    PostProcessor(Renderer* pRenderer);
    ~PostProcessor();

    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle sceneColorRT, RGHandle sceneDepthRT, RGHandle velocityRT, 
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);

    bool RequiresCameraJitter();

private:
    void UpsacleModeGui();
    bool ShouldRenderPostProcess();

private:
    Renderer* m_pRenderer = nullptr;

    eastl::unique_ptr<class TAA> m_pTAA;
    eastl::unique_ptr<class AutomaticExposure> m_pAutomaticExposure;
    eastl::unique_ptr<class DoF> m_pDoF;
    eastl::unique_ptr<class MotionBlur> m_pMotionBlur;
    eastl::unique_ptr<class Bloom> m_pBloom;
    eastl::unique_ptr<class Tonemapper> m_pToneMapper;
    eastl::unique_ptr<class FXAA> m_pFXAA;
    eastl::unique_ptr<class CAS> m_pCAS;
#if RE_PLATFORM_WINDOWS
    eastl::unique_ptr<class FSR2> m_pFSR2;
    eastl::unique_ptr<class DLSS> m_pDLSS;
    eastl::unique_ptr<class XeSS> m_pXeSS;
#elif RE_PLATFORM_MAC || RE_PLATFORM_IOS
    eastl::unique_ptr<class MetalFXTemporalUpscaler> m_pMetalFXTemporalUpscaler;
#endif
};
