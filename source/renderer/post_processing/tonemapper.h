#pragma once

#include "../render_graph.h"

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, RenderGraphHandle exposure, 
        RenderGraphHandle bloom, float bloom_intensity, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* exposure, IGfxDescriptor* pLdrUAV,
        IGfxDescriptor* bloom, float bloom_intensity, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
};