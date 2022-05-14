#pragma once

#include "../render_graph.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, uint32_t width, uint32_t height);

private:
    void Reproject(IGfxCommandList* pCommandList);
    void Prefilter(IGfxCommandList* pCommandList);
    void ResolveTemporal(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pReprojectPSO = nullptr;
    IGfxPipelineState* m_pPrefilterPSO = nullptr;
    IGfxPipelineState* m_pResolveTemporalPSO = nullptr;
};