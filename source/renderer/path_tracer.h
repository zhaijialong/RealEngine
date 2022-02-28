#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"

class PathTracer
{
public:
    PathTracer(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

private:
    void PathTrace(IGfxCommandList* pCommandList, IGfxDescriptor* diffuse, IGfxDescriptor* specular, IGfxDescriptor* normal, IGfxDescriptor* emissive, IGfxDescriptor* depth, 
        IGfxDescriptor* output, uint32_t width, uint32_t height);
    void Accumulate(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* historyUAV, IGfxDescriptor* outputUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pPathTracingPSO = nullptr;
    IGfxPipelineState* m_pAccumulationPSO = nullptr;

    std::unique_ptr<Texture2D> m_pHistoryAccumulation;

    uint m_maxRayLength = 8;
    bool m_bEnableAccumulation = true;
    bool m_bHistoryInvalid = true;
};