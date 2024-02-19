#pragma once

#include "shadow_denoiser.h"

class RTShadow
{
public:
    RTShadow(Renderer* pRenderer);

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, RGHandle velocityRT, uint32_t width, uint32_t height);

private:
    void RayTrace(IGfxCommandList* pCommandList, RGTexture* depthSRV, RGTexture* normalSRV, RGTexture* shadowUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pRaytracePSO = nullptr;

    bool m_bEnableDenoiser = true;
    eastl::unique_ptr<ShadowDenoiser> m_pDenoiser;
};