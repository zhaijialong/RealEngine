#pragma once

#include "reflection_denoiser.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void ClassifyTiles(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* normal, IGfxDescriptor* rayListUAV, IGfxDescriptor* tileListUAV, IGfxDescriptor* rayCounterUAV, uint32_t width, uint32_t height);
    void PrepareIndirectArgs(IGfxCommandList* pCommandList, IGfxDescriptor* rayCounterSRV, IGfxDescriptor* indirectArgsUAV, IGfxDescriptor* denoiserArgsUAV);
    void SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, IGfxDescriptor* rayCounter, IGfxDescriptor* rayList, IGfxBuffer* indirectArgs);

private:
    Renderer* m_pRenderer;
    eastl::unique_ptr<ReflectionDenoiser> m_pDenoiser;

    IGfxPipelineState* m_pTileClassificationPSO = nullptr;
    IGfxPipelineState* m_pPrepareIndirectArgsPSO = nullptr;
    IGfxPipelineState* m_pSSRPSO = nullptr;

    bool m_bEnable = true;
    bool m_bHalfResolution = true;
    bool m_bEnableDenoiser = true;

    uint m_samplesPerQuad = 1;
};