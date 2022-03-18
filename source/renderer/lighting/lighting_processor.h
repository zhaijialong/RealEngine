#pragma once

#include "gtao.h"
#include "ray_traced_shadow.h"
#include "clustered_shading.h"
#include "hybrid_stochastic_reflection.h"

class LightingProcessor
{
public:
    LightingProcessor(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle velocity, uint32_t width, uint32_t height);
    
private:
    RenderGraphHandle CompositeLight(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle ao, RenderGraphHandle direct_lighting, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<GTAO> m_pGTAO;
    eastl::unique_ptr<RTShadow> m_pRTShdow;
    eastl::unique_ptr<HybridStochasticReflection> m_pReflection;
    eastl::unique_ptr<ClusteredShading> m_pClusteredShading;
};