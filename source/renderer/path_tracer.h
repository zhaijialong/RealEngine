#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"

class PathTracer
{
public:
    PathTracer(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depth, uint32_t width, uint32_t height);

private:
    void PathTrace(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal, RGTexture* emissive, RGTexture* depth, 
        RGTexture* output, uint32_t width, uint32_t height);
    void Accumulate(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* outputUAV, uint32_t width, uint32_t height);

    void RenderProgressBar();

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pPathTracingPSO = nullptr;
    IGfxPipelineState* m_pAccumulationPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pHistoryAccumulation;

    uint m_maxRayLength = 4;
    uint m_spp = 256;
    uint m_currentSampleIndex = 0;
    bool m_bEnableAccumulation = true;
    bool m_bHistoryInvalid = true;
};