#pragma once

#include "../render_graph.h"

class DOF
{
public:
    DOF(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth);
};