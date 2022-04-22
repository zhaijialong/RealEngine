#pragma once

#include "reflection_denoiser.h"

class HybridStochasticReflection
{
public:
    HybridStochasticReflection(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle linear_depth, RenderGraphHandle normal, RenderGraphHandle velocity, uint32_t width, uint32_t height);

private:
    void SSR(IGfxCommandList* pCommandList, IGfxDescriptor* normal, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* outputUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;
    eastl::unique_ptr<ReflectionDenoiser> m_pDenoiser;

    IGfxPipelineState* m_pSSRPSO = nullptr;

    bool m_bEnable = true;
    bool m_bHalfResolution = true;
    bool m_bEnableDenoiser = true;
};