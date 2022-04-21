#pragma once

#include "../render_graph.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* hzb, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pSSRPSO = nullptr;

    bool m_bEnable = true;
};