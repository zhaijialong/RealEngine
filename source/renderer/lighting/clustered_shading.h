#pragma once

#include "../render_graph.h"

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle diffuse, RGHandle specular, RGHandle normal,
        RGHandle customData, RGHandle depth, RGHandle shadow, uint32_t width, uint32_t height);

private:
    void Render(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal,
        RGTexture* customData, RGTexture* depth, RGTexture* shadow, RGTexture* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};