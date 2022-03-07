#pragma once

#include "render_graph.h"

class SkyCubeMap
{
public:
    SkyCubeMap(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph);

private:
    Renderer* m_pRenderer;
};