#include "hybrid_stochastic_reflection.h"
#include "../renderer.h"

HybridStochasticReflection::HybridStochasticReflection(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle HybridStochasticReflection::Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "HybridStochasticReflection");

    return RenderGraphHandle();
}
