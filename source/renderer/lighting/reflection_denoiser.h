#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    void ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, uint32_t width, uint32_t height);

private:
    void Reproject(IGfxCommandList* pCommandList);
    void Prefilter(IGfxCommandList* pCommandList);
    void ResolveTemporal(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pReprojectPSO = nullptr;
    IGfxPipelineState* m_pPrefilterPSO = nullptr;
    IGfxPipelineState* m_pResolveTemporalPSO = nullptr;

    bool m_bHistoryInvalid = true;
    eastl::unique_ptr<Texture2D> m_pRandianceHistory;
    eastl::unique_ptr<Texture2D> m_pVarianceHistory;
    eastl::unique_ptr<Texture2D> m_pSampleCountInput;
    eastl::unique_ptr<Texture2D> m_pSampleCountOutput;

    RenderGraphHandle m_randianceHistory;
    RenderGraphHandle m_varianceHistory;
    RenderGraphHandle m_sampleCountInput;
    RenderGraphHandle m_sampleCountOutput;
};