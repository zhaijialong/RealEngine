#pragma once

#include "reflection_denoiser.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height);

    IGfxDescriptor* GetOutputRadianceSRV() const { return m_pDenoiser->GetHistoryRadianceSRV(); }
    float GetMaxRoughness() const { return m_maxRoughness; }

private:
    void ClassifyTiles(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* normal,
        RGBuffer* rayListUAV, RGBuffer* tileListUAV, RGBuffer* rayCounterUAV, uint32_t width, uint32_t height);
    void PrepareIndirectArgs(IGfxCommandList* pCommandList, RGBuffer* rayCounterSRV, RGBuffer* indirectArgsUAV, RGBuffer* denoiserArgsUAV);
    void SSR(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* depth, RGTexture* velocity, RGTexture* outputUAV,
        RGBuffer* rayCounter, RGBuffer* rayList, RGBuffer* indirectArgs, RGBuffer* hwRayCounterUAV, RGBuffer* hwRayListUAV);

    void PrepareRaytraceIndirectArgs(IGfxCommandList* pCommandList, RGBuffer* rayCounterSRV, RGBuffer* indirectArgsUAV);
    void Raytrace(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* depth, RGTexture* outputUAV,
        RGBuffer* rayCounter, RGBuffer* rayList, RGBuffer* indirectArgs);

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
    bool m_bEnableSWRay = true;
    bool m_bEnableHWRay = true;
    bool m_bEnableDenoiser = true;

    uint m_samplesPerQuad = 1;
    float m_ssrThickness = 0.1f;
    float m_maxRoughness = 0.6f;
    float m_temporalStability = 0.5f;
};