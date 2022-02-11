#pragma once

#include "../render_graph.h"

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* pLdrUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};