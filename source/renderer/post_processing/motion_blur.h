#pragma once

#include "../render_graph.h"

class MotionBlur
{
public:
    MotionBlur(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth, RGHandle velocity);
};