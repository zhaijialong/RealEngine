#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"

class PathTracer
{
public:
    PathTracer(Renderer* pRenderer);
    ~PathTracer();

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depth, uint32_t width, uint32_t height);

private:
    void PathTrace(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal, RGTexture* emissive, RGTexture* depth, 
        RGTexture* output, uint32_t width, uint32_t height);
    void Accumulate(IGfxCommandList* pCommandList, 
        RGTexture* inputColor, RGTexture* outputColor, 
        RGTexture* inputAlbedo, RGTexture* outputAlbedo,
        RGTexture* inputNormal, RGTexture* outputNormal,
        uint32_t width, uint32_t height);

    void RenderProgressBar();

private:
    Renderer* m_pRenderer;
    eastl::unique_ptr<class OIDN> m_denoiser;

    IGfxPipelineState* m_pPathTracingPSO = nullptr;
    IGfxPipelineState* m_pAccumulationPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pHistoryColor;
    eastl::unique_ptr<Texture2D> m_pHistoryAlbedo;
    eastl::unique_ptr<Texture2D> m_pHistoryNormal;

    uint m_maxRayLength = 4;
    uint m_spp = 256;
    uint m_currentSampleIndex = 0;
    bool m_bEnableAccumulation = true;
    bool m_bHistoryInvalid = true;
};