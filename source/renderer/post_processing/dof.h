#pragma once

#include "../render_graph.h"

class DoF
{
public:
    DoF(Renderer* pRenderer);

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    bool m_bEnable = true;
};