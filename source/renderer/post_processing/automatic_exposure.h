#pragma once

#include "../render_graph.h"
#include "../resource/typed_buffer.h"

class AutomaticExposure
{
public:
    AutomaticExposure(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, uint32_t width, uint32_t height);

    uint32_t GetAverageLuminanceMip() const { return m_luminanceMips - 1; }

private:
    void ComputeLuminanceSize(uint32_t width, uint32_t height);
    void InitLuminance(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output);
    void ReduceLuminance(IGfxCommandList* pCommandList, RenderGraphTexture* texture);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitLuminancePSO = nullptr;
    IGfxPipelineState* m_pLuminanceReductionPSO = nullptr;

    std::unique_ptr<TypedBuffer> m_pSPDCounterBuffer;

    bool m_bEnable = true;

    uint2 m_luminanceSize;
    uint32_t m_luminanceMips = 0;

    float m_minLuminance = 0.001f;
    float m_maxLuminance = 8.0f;
};