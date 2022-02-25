#pragma once

#include "../render_graph.h"

class Bloom
{
public:
    Bloom(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, uint32_t width, uint32_t height);

    float GetIntensity() const { return m_intensity; }

private:
    RenderGraphHandle DownsamplePass(RenderGraph* pRenderGraph, RenderGraphHandle input, uint32_t mip);
    RenderGraphHandle UpsamplePass(RenderGraph* pRenderGraph, RenderGraphHandle highInput, RenderGraphHandle lowInput, uint32_t mip);

    void Downsample(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t mip);
    void Upsample(IGfxCommandList* pCommandList, IGfxDescriptor* highInput, IGfxDescriptor* lowInput, IGfxDescriptor* output, uint32_t mip);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pFirstDownsamplePSO = nullptr;
    IGfxPipelineState* m_pDownsamplePSO = nullptr;
    IGfxPipelineState* m_pUpsamplePSO = nullptr;

    uint2 m_mipSize[7];

    bool m_bEnable = true;
    float m_threshold = 1.0f;
    float m_blurRadius = 1.0f;
    float m_intensity = 1.0f;
};