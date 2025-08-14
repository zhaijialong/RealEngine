#pragma once

#include "render_graph.h"

class VolumetricCloud
{
public:
    VolumetricCloud(Renderer* pRenderer);
    ~VolumetricCloud();
    
    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height);
    
private:
    Renderer* m_pRenderer = nullptr;
};
