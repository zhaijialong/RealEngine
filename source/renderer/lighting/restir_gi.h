#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, uint32_t width, uint32_t height);

private:
    void InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, 
        IGfxDescriptor* outputIrradiance, IGfxDescriptor* outputHitNormal, IGfxDescriptor* outputRay, uint32_t width, uint32_t height);
    void TemporalResampling();
    void SpatialResampling();

    void InitTemporalBuffers(uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;

    struct TemporalReservoirBuffer
    {
        eastl::unique_ptr<Texture2D> position; // rgba32f - xyz : position, a : hitT
        eastl::unique_ptr<Texture2D> normal; //rg16unorm
        eastl::unique_ptr<Texture2D> rayDirection; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleNormal; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleRadiance; //r11g11b10f
        eastl::unique_ptr<Texture2D> reservoir; //rgba16f - w_sum/M/W
    };

    TemporalReservoirBuffer m_temporalReservoir[2];

    bool m_bEnable = true;
    bool m_bEnableReSTIR = false;
    bool m_bEnableDenoiser = true;
};