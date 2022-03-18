#pragma once

#include "../render_graph.h"

class RTShadow
{
public:
    RTShadow(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, uint32_t width, uint32_t height);

private:
    void RayTrace(IGfxCommandList* pCommandList, IGfxDescriptor* depthSRV, IGfxDescriptor* normalSRV, IGfxDescriptor* shadowUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pRaytracePSO = nullptr;
    bool m_bEnableDenoiser = true;
};