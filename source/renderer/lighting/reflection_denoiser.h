#pragma once

#include "../render_graph.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph);

private:
    void Reproject(IGfxCommandList* pCommandList);
    void Prefilter(IGfxCommandList* pCommandList);
    void ResolveTemporal(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
};