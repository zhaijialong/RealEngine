#pragma once

#include "../render_graph.h"

class FSR2
{
public:
    FSR2(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle input, uint32_t renderWidth, uint32_t renderHeight);

private:
    Renderer* m_pRenderer = nullptr;
};
