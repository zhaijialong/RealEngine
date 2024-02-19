#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

// Reference :
// "Fast Denoising with Self Stabilizing Recurrent Blurs", Dmitry Zhdan, 2020
// "ReBLUR: A Hierarchical Recurrent Denoiser", Dmitry Zhdan, Ray Tracing Gems II Chapter 49

// thanks to ReSTIR we can have good enough IQ without the "post blur" and "temporal stabilization" passes in the original implementation

class GIDenoiser
{
public:
    GIDenoiser(Renderer* pRenderer);

    void ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle variance, 
        RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    RGHandle GetHistoryIrradiance() const { return m_historyIrradiance; }
    IGfxDescriptor* GetHistoryIrradianceSRV() const { return m_pHistoryIrradiance->GetSRV(); }

    void InvalidateHistory();

private:
    void PreBlur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* variance, RGTexture* depth, RGTexture* normal, 
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void TemporalAccumulation(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void MipGeneration(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* outputMips, uint32_t width, uint32_t height, bool depth);
    void HistoryReconstruction(IGfxCommandList* pCommandList, RGTexture* inputSHMips, RGTexture* linearDepthMips, RGTexture* depth, RGTexture* normal,
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void Blur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, uint32_t width, uint32_t height);
    //void PostBlur(IGfxCommandList* pCommandList);
    //void TemporalStabilization(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPreBlurPSO = nullptr;
    IGfxPipelineState* m_pTemporalAccumulationPSO = nullptr;
    IGfxPipelineState* m_pMipGenerationPSO = nullptr;
    IGfxPipelineState* m_pMipGenerationDepthPSO = nullptr;
    IGfxPipelineState* m_pHistoryReconstructionPSO = nullptr;
    IGfxPipelineState* m_pBlurPSO = nullptr;

    bool m_bHistoryInvalid = true;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationSH;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount0;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount1;

    RGHandle m_historyIrradiance;
    eastl::unique_ptr<Texture2D> m_pHistoryIrradiance;
};