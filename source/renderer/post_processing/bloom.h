#pragma once

#include "../render_graph.h"

class Bloom
{
public:
    Bloom(Renderer* pRenderer);

    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle sceneColorRT, uint32_t width, uint32_t height);

    float GetIntensity() const { return m_intensity; }

private:
    RGHandle DownsamplePass(RenderGraph* pRenderGraph, RGHandle input, uint32_t mip);
    RGHandle UpsamplePass(RenderGraph* pRenderGraph, RGHandle highInput, RGHandle lowInput, uint32_t mip);

    void Downsample(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t mip);
    void Upsample(IGfxCommandList* pCommandList, RGTexture* highInput, RGTexture* lowInput, RGTexture* output, uint32_t mip);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pFirstDownsamplePSO = nullptr;
    IGfxPipelineState* m_pDownsamplePSO = nullptr;
    IGfxPipelineState* m_pUpsamplePSO = nullptr;

    uint2 m_mipSize[7];

    bool m_bEnable = true;
    float m_threshold = 3.0f;
    float m_intensity = 0.5f;
};