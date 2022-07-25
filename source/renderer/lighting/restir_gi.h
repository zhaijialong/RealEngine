#pragma once

#include "../render_graph.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, uint32_t width, uint32_t height);

private:
    void InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* output, uint32_t width, uint32_t height);
    void TemporalResampling();
    void SpatialResampling();

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;

    bool m_bEnable = true;
    bool m_bEnableReSTIR = true;
    bool m_bEnableDenoiser = true;
};