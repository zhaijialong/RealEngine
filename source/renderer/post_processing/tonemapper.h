#pragma once

#include "../render_graph.h"

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, RenderGraphHandle avgLuminance, uint32_t avgLuminanceMip, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* pAvgLuminance, uint32_t avgLuminanceMip, IGfxDescriptor* pLdrUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
};