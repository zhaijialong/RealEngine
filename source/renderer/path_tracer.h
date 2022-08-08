#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"

class PathTracer
{
public:
    PathTracer(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, uint32_t width, uint32_t height);

private:
    void PathTrace(IGfxCommandList* pCommandList, RenderGraphTexture* diffuse, RenderGraphTexture* specular, RenderGraphTexture* normal, RenderGraphTexture* emissive, RenderGraphTexture* depth, 
        RenderGraphTexture* output, uint32_t width, uint32_t height);
    void Accumulate(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* outputUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pPathTracingPSO = nullptr;
    IGfxPipelineState* m_pAccumulationPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pHistoryAccumulation;

    uint m_maxRayLength = 8;
    uint m_nAccumulatedFrames = 0;
    bool m_bEnableAccumulation = true;
    bool m_bHistoryInvalid = true;
};