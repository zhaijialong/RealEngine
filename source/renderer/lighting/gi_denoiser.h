#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class GIDenoiser
{
public:
    GIDenoiser(Renderer* pRenderer);

    void ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);
    RGHandle Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

    RGHandle GetHistoryRadiance() const { return RGHandle(); }
    IGfxDescriptor* GetHistoryRadianceSRV() const { return nullptr; }

private:
    Renderer* m_pRenderer;
};