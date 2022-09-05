#pragma once

#include "gi_denoiser.h"

class ReSTIRGI
{
public:
    ReSTIRGI(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle halfDepthNormal, RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pDenoiser->GetHistoryRadianceSRV(); }

private:
    void InitialSampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal, RGTexture* outputRadiance, RGTexture* outputRayDirection, uint32_t width, uint32_t height);
    void TemporalResampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal, RGTexture* velocity, 
        RGTexture* candidateRadiance, RGTexture* candidateRayDirection, uint32_t width, uint32_t height, bool historyInvalid);
    void SpatialResampling(IGfxCommandList* pCommandList, RGTexture* halfDepthNormal,
        IGfxDescriptor* inputReservoirSampleRadiance, IGfxDescriptor* inputReservoirRayDirection, IGfxDescriptor* inputReservoir,
        RGTexture* outputReservoirSampleRadiance, RGTexture* outputReservoirRayDireciton, RGTexture* outputReservoir, uint32_t width, uint32_t height, uint32_t pass_index);
    void Resolve(IGfxCommandList* pCommandList, RGTexture* reservoir, RGTexture* radiance, RGTexture* rayDirection, RGTexture* halfDepthNormal, RGTexture* depth, RGTexture* normal,
        RGTexture* output, RGTexture* outputVariance, uint32_t width, uint32_t height);

    bool InitTemporalBuffers(uint32_t width, uint32_t height);
    void ResetTemporalBuffers();

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pInitialSamplingPSO = nullptr;
    IGfxPipelineState* m_pTemporalResamplingPSO = nullptr;
    IGfxPipelineState* m_pSpatialResamplingPSO = nullptr;

    struct TemporalReservoirBuffer
    {
        eastl::unique_ptr<Texture2D> sampleRadiance; //r11g11b10f
        eastl::unique_ptr<Texture2D> rayDirection;
        eastl::unique_ptr<Texture2D> depthNormal;
        eastl::unique_ptr<Texture2D> reservoir; //rg16f - M/W
    };

    TemporalReservoirBuffer m_temporalReservoir[2];

    eastl::unique_ptr<GIDenoiser> m_pDenoiser;

    bool m_bEnable = true;
    bool m_bEnableReSTIR = true;
    bool m_bEnableDenoiser = true;
};
