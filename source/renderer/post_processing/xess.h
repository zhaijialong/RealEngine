#pragma once

#include "../render_graph.h"

class XeSS
{
public:
    XeSS(Renderer* pRenderer);
    ~XeSS();

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);

    float GetUpscaleRatio() const;

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);


private:
    Renderer* m_pRenderer = nullptr;
};