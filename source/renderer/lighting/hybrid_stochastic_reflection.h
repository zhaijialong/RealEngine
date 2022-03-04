#pragma once

#include "../render_graph.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;
};