#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void InitialSampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, 
        IGfxDescriptor* outputIrradiance, IGfxDescriptor* outputHitNormal, IGfxDescriptor* outputRay, uint32_t width, uint32_t height);
    void TemporalResampling(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* velocity,
        IGfxDescriptor* candidateIrradiance, IGfxDescriptor* candidateHitNormal, IGfxDescriptor* candidateRay, uint32_t width, uint32_t height);
    void SpatialResampling();

    void Resolve(IGfxCommandList* pCommandList, IGfxDescriptor* reservoir, IGfxDescriptor* irradiance, IGfxDescriptor* rayDirection, IGfxDescriptor* normal,
        IGfxDescriptor* output, uint32_t width, uint32_t height);

    bool InitTemporalBuffers(uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;
    IGfxPipelineState* m_pTemporalResamplingPSO = nullptr;
    IGfxPipelineState* m_pResolvePSO = nullptr;

    struct TemporalReservoirBuffer
    {
        eastl::unique_ptr<Texture2D> position; // rgba32f - xyz : position, a : hitT
        eastl::unique_ptr<Texture2D> normal; //rg16unorm
        eastl::unique_ptr<Texture2D> rayDirection; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleNormal; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleRadiance; //r11g11b10f
        eastl::unique_ptr<Texture2D> reservoir; //rg16f - M/W
    };

    TemporalReservoirBuffer m_temporalReservoir[2];

    bool m_bEnable = true;
    bool m_bSecondBounce = false;
    bool m_bEnableReSTIR = true;
    bool m_bEnableDenoiser = false;
};