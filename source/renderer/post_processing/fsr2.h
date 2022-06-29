#pragma once

#include "../render_graph.h"
#include "FSR2/ffx_fsr2.h"

class FSR2
{
public:
    FSR2(Renderer* pRenderer);
    ~FSR2();

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depth, RenderGraphHandle velocity,
        uint32_t displayWidth, uint32_t displayHeight);

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);
    
    void CreateFsr2Context(uint32_t displayWidth, uint32_t displayHeight);
    void DestroyFsr2Context();

private:
    Renderer* m_pRenderer = nullptr;

    FfxFsr2ContextDescription m_desc = {};
    FfxFsr2Context m_context;

    float m_sharpness = 0.5;
};
