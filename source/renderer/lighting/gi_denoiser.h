#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

// Reference :
// "Fast Denoising with Self Stabilizing Recurrent Blurs", Dmitry Zhdan, 2020
// "ReBLUR: A Hierarchical Recurrent Denoiser", Dmitry Zhdan, Ray Tracing Gems II Chapter 49

class GIDenoiser
{
public:
    GIDenoiser(Renderer* pRenderer);

    void ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);
    RGHandle Render(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    RGHandle GetHistoryRadiance() const { return m_historyRadiance; }
    IGfxDescriptor* GetHistoryRadianceSRV() const { return m_pHistoryRadiance1->GetSRV(); }

private:
    void PreBlur(IGfxCommandList* pCommandList);
    void TemporalAccumulation(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
        RGTexture* outputSH, uint32_t width, uint32_t height);
    void Blur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, uint32_t width, uint32_t height);
    void PostBlur(IGfxCommandList* pCommandList);
    void TemporalStabilization(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pTemporalAccumulationPSO = nullptr;
    IGfxPipelineState* m_pBlurPSO = nullptr;

    bool m_bHistoryInvalid = true;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationSH;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount0;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulationCount1;

    RGHandle m_historyRadiance;
    //eastl::unique_ptr<Texture2D> m_pHistoryRadiance0;
    eastl::unique_ptr<Texture2D> m_pHistoryRadiance1;
};