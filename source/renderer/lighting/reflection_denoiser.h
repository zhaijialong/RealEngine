#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    void ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

    bool IsHistoryValid() const { return !m_bHistoryInvalid; }
    void InvalidateHistory();

    RGHandle GetHistoryVariance() const { return m_varianceHistory; }
    IGfxDescriptor* GetHistoryVarianceSRV() const { return m_pVarianceHistory->GetSRV(); }

    RGHandle GetHistoryRadiance() const { return m_radianceHistory; }
    IGfxDescriptor* GetHistoryRadianceSRV() const { return m_pRadianceHistory->GetSRV(); }

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle indirectArgs, RGHandle tileListBuffer, RGHandle input, 
        RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height,
        float maxRoughness, float temporalStability);

private:
    void Reproject(IGfxCommandList* pCommandList, RGBuffer* indirectArgs, RGBuffer* tileList,
        RGTexture* depth, RGTexture* linearDepth, RGTexture* normal, RGTexture* velocity, RGTexture* inputRadiance, 
        RGTexture* outputRadianceUAV, RGTexture* outputVariancceUAV, RGTexture* outputAvgRadianceUAV);
    void Prefilter(IGfxCommandList* pCommandList, RGBuffer* indirectArgs, RGBuffer* tileList,
        RGTexture* linear_depth, RGTexture* normal, RGTexture* inputRadiance, RGTexture* inputVariance, RGTexture* avgRadiance,
        RGTexture* outputRadianceUAV, RGTexture* outputVariancceUAV);
    void ResolveTemporal(IGfxCommandList* pCommandList, RGBuffer* indirectArgs, RGBuffer* tileList,
        RGTexture* normal, RGTexture* inputRadiance, RGTexture* inputVariance, RGTexture* reprojectedRadiance, RGTexture* avgRadiance);

private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pReprojectPSO = nullptr;
    IGfxPipelineState* m_pPrefilterPSO = nullptr;
    IGfxPipelineState* m_pResolveTemporalPSO = nullptr;

    bool m_bHistoryInvalid = true;
    eastl::unique_ptr<Texture2D> m_pRadianceHistory;
    eastl::unique_ptr<Texture2D> m_pVarianceHistory;
    eastl::unique_ptr<Texture2D> m_pSampleCountInput;
    eastl::unique_ptr<Texture2D> m_pSampleCountOutput;

    RGHandle m_radianceHistory;
    RGHandle m_varianceHistory;
    RGHandle m_sampleCountInput;
    RGHandle m_sampleCountOutput;
};