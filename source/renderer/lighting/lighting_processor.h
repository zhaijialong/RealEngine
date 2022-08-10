#pragma once

#include "gtao.h"
#include "ray_traced_shadow.h"
#include "clustered_shading.h"
#include "hybrid_stochastic_reflection.h"
#include "restir_gi.h"

class LightingProcessor
{
public:
    LightingProcessor(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle velocity, uint32_t width, uint32_t height);
    
private:
    RGHandle CompositeLight(RenderGraph* pRenderGraph, RGHandle depth, RGHandle ao, RGHandle direct_lighting, 
        RGHandle indirect_specular, RGHandle indirect_diffuse, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<GTAO> m_pGTAO;
    eastl::unique_ptr<RTShadow> m_pRTShdow;
    eastl::unique_ptr<HybridStochasticReflection> m_pReflection;
    eastl::unique_ptr<ReSTIRGI> m_pReSTIRGI;
    eastl::unique_ptr<ClusteredShading> m_pClusteredShading;
};