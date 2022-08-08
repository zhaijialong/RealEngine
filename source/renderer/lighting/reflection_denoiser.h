#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    void ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);

    RenderGraphHandle GetHistoryVariance() const { return m_varianceHistory; }
    IGfxDescriptor* GetHistoryVarianceSRV() const { return m_pVarianceHistory->GetSRV(); }
    bool IsHistoryValid() const { return !m_bHistoryInvalid; }

    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pRadianceHistory->GetSRV(); }

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, 
        RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height,
        float maxRoughness, float temporalStability);

private:
    void Reproject(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
        RenderGraphTexture* depth, RenderGraphTexture* linearDepth, RenderGraphTexture* normal, RenderGraphTexture* velocity, RenderGraphTexture* inputRadiance, 
        RenderGraphTexture* outputRadianceUAV, RenderGraphTexture* outputVariancceUAV, RenderGraphTexture* outputAvgRadianceUAV);
    void Prefilter(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
        RenderGraphTexture* linear_depth, RenderGraphTexture* normal, RenderGraphTexture* inputRadiance, RenderGraphTexture* inputVariance, RenderGraphTexture* avgRadiance,
        RenderGraphTexture* outputRadianceUAV, RenderGraphTexture* outputVariancceUAV);
    void ResolveTemporal(IGfxCommandList* pCommandList, RenderGraphBuffer* indirectArgs, RenderGraphBuffer* tileList,
        RenderGraphTexture* normal, RenderGraphTexture* inputRadiance, RenderGraphTexture* inputVariance, RenderGraphTexture* reprojectedRadiance, RenderGraphTexture* avgRadiance);

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

    RenderGraphHandle m_radianceHistory;
    RenderGraphHandle m_varianceHistory;
    RenderGraphHandle m_sampleCountInput;
    RenderGraphHandle m_sampleCountOutput;
};