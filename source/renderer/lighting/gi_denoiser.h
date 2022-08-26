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
    RGHandle Render(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle variance, 
        RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    RGHandle GetHistoryRadiance() const { return m_historyRadiance; }
    IGfxDescriptor* GetHistoryRadianceSRV() const { return m_pHistoryRadiance->GetSRV(); }

private:
    void PreBlur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* variance, RGTexture* depth, RGTexture* normal, 
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void TemporalAccumulation(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void MipGeneration(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* outputSHMips, uint32_t width, uint32_t height);
    void HistoryReconstruction(IGfxCommandList* pCommandList, RGTexture* inputSHMips, RGTexture* outputSH, uint32_t width, uint32_t height);
    void Blur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, uint32_t width, uint32_t height);
    //void PostBlur(IGfxCommandList* pCommandList);
    //void TemporalStabilization(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPreBlurPSO = nullptr;
    IGfxPipelineState* m_pTemporalAccumulationPSO = nullptr;
    IGfxPipelineState* m_pMipGenerationPSO = nullptr;
    IGfxPipelineState* m_pHistoryReconstructionPSO = nullptr;
    IGfxPipelineState* m_pBlurPSO = nullptr;

    bool m_bHistoryInvalid = true;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationSH;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount0;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount1;

    RGHandle m_historyRadiance;
    eastl::unique_ptr<Texture2D> m_pHistoryRadiance;
};