#pragma once

#include "reflection_denoiser.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pDenoiser->GetOutputRadianceSRV(); }
    float GetMaxRoughness() const { return m_maxRoughness; }

private:
    void ClassifyTiles(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* historyVariance,
        IGfxDescriptor* rayListUAV, IGfxDescriptor* tileListUAV, IGfxDescriptor* rayCounterUAV, uint32_t width, uint32_t height);
    void PrepareIndirectArgs(IGfxCommandList* pCommandList, IGfxDescriptor* rayCounterSRV, IGfxDescriptor* indirectArgsUAV, IGfxDescriptor* denoiserArgsUAV);
    void SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, 
        IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs, IGfxDescriptor* hwRayCounterUAV, IGfxDescriptor* hwRayListUAV);

    void PrepareRaytraceIndirectArgs(IGfxCommandList* pCommandList, IGfxDescriptor* rayCounterSRV, IGfxDescriptor* indirectArgsUAV);
    void Raytrace(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* outputUAV,
        IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs);

    void SetRootConstants(IGfxCommandList* pCommandList);
private:
    Renderer* m_pRenderer;
    eastl::unique_ptr<ReflectionDenoiser> m_pDenoiser;

    IGfxPipelineState* m_pTileClassificationPSO = nullptr;
    IGfxPipelineState* m_pPrepareIndirectArgsPSO = nullptr;
    IGfxPipelineState* m_pSSRPSO = nullptr;

    IGfxPipelineState* m_pPrepareRTArgsPSO = nullptr;
    IGfxPipelineState* m_pRaytracePSO = nullptr;

    bool m_bEnable = true;
    bool m_bEnableDenoiser = true;

    uint m_samplesPerQuad = 1;
    float m_maxRoughness = 0.6f;
    float m_temporalStability = 0.7f;
};