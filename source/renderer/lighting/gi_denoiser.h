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
    RGHandle Render(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height);

    RGHandle GetHistoryRadiance() const { return m_historyRadiance; }
    IGfxDescriptor* GetHistoryRadianceSRV() const { return m_pTemporalAccumulation1->GetSRV(); }

private:
    void PreBlur(IGfxCommandList* pCommandList);
    void TemporalAccumulation(IGfxCommandList* pCommandList);
    void Blur(IGfxCommandList* pCommandList);
    void PostBlur(IGfxCommandList* pCommandList);
    void TemporalStabilization(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pTemporalAccumulationPSO = nullptr;

    bool m_bHistoryInvalid = true;
    RGHandle m_historyRadiance;

    eastl::unique_ptr<Texture2D> m_pTemporalAccumulation0;
    eastl::unique_ptr<Texture2D> m_pTemporalAccumulation1;

    eastl::unique_ptr<Texture2D> m_pTemporalStabilization;
};