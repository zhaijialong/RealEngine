#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ReflectionDenoiser
{
public:
    ReflectionDenoiser(Renderer* pRenderer);

    void ImportTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);
    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pRadianceHistory->GetSRV(); }

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, 
        RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void Reproject(IGfxCommandList* pCommandList, IGfxBuffer* indirectArgs, IGfxDescriptor* tileList, 
        IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* velocity, IGfxDescriptor* inputRadiance, 
        IGfxDescriptor* outputRadianceUAV, IGfxDescriptor* outputVariancceUAV, IGfxDescriptor* outputAvgRadianceUAV);
    void Prefilter(IGfxCommandList* pCommandList, IGfxBuffer* indirectArgs, IGfxDescriptor* tileList,
        IGfxDescriptor* linear_depth, IGfxDescriptor* normal, IGfxDescriptor* inputRadiance, IGfxDescriptor* inputVariance, IGfxDescriptor* avgRadiance,
        IGfxDescriptor* outputRadianceUAV, IGfxDescriptor* outputVariancceUAV);
    void ResolveTemporal(IGfxCommandList* pCommandList, IGfxBuffer* indirectArgs, IGfxDescriptor* tileList, 
        IGfxDescriptor* normal, IGfxDescriptor* inputRadiance, IGfxDescriptor* inputVariance, IGfxDescriptor* reprojectedRadiance, IGfxDescriptor* avgRadiance,
        IGfxDescriptor* sampleCount, IGfxDescriptor* outputRadianceUAV, IGfxDescriptor* outputVariancceUAV);

private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pReprojectPSO = nullptr;
    IGfxPipelineState* m_pPrefilterPSO = nullptr;
    IGfxPipelineState* m_pResolveTemporalPSO = nullptr;

    float m_temporalStability = 0.7f;

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