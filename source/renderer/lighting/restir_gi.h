#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"
#include "reflection_denoiser.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pDenoiser->GetOutputRadianceSRV(); }

private:
    void InitialSampling(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* normal,
        RGTexture* outputRadiance, RGTexture* outputHitNormal, RGTexture* outputRay, uint32_t width, uint32_t height);
    void TemporalResampling(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
        RGTexture* candidateRadiance, RGTexture* candidateHitNormal, RGTexture* candidateRay, uint32_t width, uint32_t height);
    void SpatialResampling(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* normal, 
        RGTexture* outputReservoirRayDirection, RGTexture* outputReservoirSampleRadiance, RGTexture* outputReservoir, uint32_t width, uint32_t height);

    void Resolve(IGfxCommandList* pCommandList, RGTexture* reservoir, RGTexture* radiance, RGTexture* rayDirection, RGTexture* normal,
        RGTexture* output, uint32_t width, uint32_t height);

    //void TemporalFilter(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t width, uint32_t height);
    //void SpatialFilter(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t width, uint32_t height);

    bool InitTemporalBuffers(uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;
    IGfxPipelineState* m_pTemporalResamplingPSO = nullptr;
    IGfxPipelineState* m_pSpatialResamplingPSO = nullptr;
    IGfxPipelineState* m_pResolvePSO = nullptr;
    IGfxPipelineState* m_pTemporalFilterPSO = nullptr;
    IGfxPipelineState* m_pSpatialFilterPSO = nullptr;

    struct TemporalReservoirBuffer
    {
        eastl::unique_ptr<Texture2D> rayDirection; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleNormal; //rg16unorm
        eastl::unique_ptr<Texture2D> sampleRadiance; //rgba16f, w: hitT
        eastl::unique_ptr<Texture2D> reservoir; //rg16f - M/W
    };

    TemporalReservoirBuffer m_temporalReservoir[2];

    //eastl::unique_ptr<Texture2D> m_pHistoryRadiance;
    eastl::unique_ptr<ReflectionDenoiser> m_pDenoiser;

    bool m_bEnable = false;
    bool m_bSecondBounce = false;
    bool m_bEnableReSTIR = true;
    bool m_bEnableDenoiser = true;
};