#pragma once

#include "taa.h"
#include "automatic_exposure.h"
#include "bloom.h"
#include "tonemapper.h"
#include "fxaa.h"
#include "cas.h"
#include "fsr2.h"
#include "dlss.h"
#include "xess.h"

class PostProcessor
{
public:
    PostProcessor(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColorRT, RGHandle sceneDepthRT,
        RGHandle linearDepthRT, RGHandle velocityRT, 
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);

    bool RequiresCameraJitter();

private:
    void UpdateUpsacleMode();

private:
    Renderer* m_pRenderer = nullptr;

    eastl::unique_ptr<TAA> m_pTAA;
    eastl::unique_ptr<AutomaticExposure> m_pAutomaticExposure;
    eastl::unique_ptr<Bloom> m_pBloom;
    eastl::unique_ptr<Tonemapper> m_pToneMapper;
    eastl::unique_ptr<FXAA> m_pFXAA;
    eastl::unique_ptr<CAS> m_pCAS;
    eastl::unique_ptr<FSR2> m_pFSR2;
    eastl::unique_ptr<DLSS> m_pDLSS;
    eastl::unique_ptr<XeSS> m_pXeSS;
};