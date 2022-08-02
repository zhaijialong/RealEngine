#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void InitialSampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal,
        RenderGraphTexture* outputRadiance, RenderGraphTexture* outputHitNormal, RenderGraphTexture* outputRay, uint32_t width, uint32_t height);
    void TemporalResampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal, RenderGraphTexture* velocity,
        RenderGraphTexture* candidateRadiance, RenderGraphTexture* candidateHitNormal, RenderGraphTexture* candidateRay, uint32_t width, uint32_t height);
    void SpatialResampling(IGfxCommandList* pCommandList, RenderGraphTexture* depth, RenderGraphTexture* normal, 
        RenderGraphTexture* outputReservoirRayDirection, RenderGraphTexture* outputReservoirSampleRadiance, RenderGraphTexture* outputReservoir, uint32_t width, uint32_t height);

    void Resolve(IGfxCommandList* pCommandList, RenderGraphTexture* reservoir, RenderGraphTexture* radiance, RenderGraphTexture* rayDirection, RenderGraphTexture* normal,
        RenderGraphTexture* output, uint32_t width, uint32_t height);

    bool InitTemporalBuffers(uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;
    IGfxPipelineState* m_pTemporalResamplingPSO = nullptr;
    IGfxPipelineState* m_pSpatialResamplingPSO = nullptr;
    IGfxPipelineState* m_pResolvePSO = nullptr;

    struct TemporalReservoirBuffer
    {
        eastl::unique_ptr<Texture2D> rayDirection; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleNormal; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleRadiance; //rgba16f, w: hitT
        eastl::unique_ptr<Texture2D> reservoir; //rg16f - M/W
    };

    TemporalReservoirBuffer m_temporalReservoir[2];

    bool m_bEnable = true;
    bool m_bSecondBounce = false;
    bool m_bEnableReSTIR = true;
    bool m_bEnableDenoiser = false;
};