#pragma once

#include "../render_graph.h"

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle diffuse, RenderGraphHandle specular, RenderGraphHandle normal,
        RenderGraphHandle depth, RenderGraphHandle shadow, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* diffuse, IGfxDescriptor* specular, IGfxDescriptor* normal,
        IGfxDescriptor* depth, IGfxDescriptor* shadow, IGfxDescriptor* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};