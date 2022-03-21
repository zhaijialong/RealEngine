#pragma once

#include "shadow_denoiser.h"

class RTShadow
{
public:
    RTShadow(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height);

private:
    void RayTrace(IGfxCommandList* pCommandList, IGfxDescriptor* depthSRV, IGfxDescriptor* normalSRV, IGfxDescriptor* shadowUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pRaytracePSO = nullptr;

    bool m_bEnableDenoiser = true;
    eastl::unique_ptr<ShadowDenoiser> m_pDenoiser;
};