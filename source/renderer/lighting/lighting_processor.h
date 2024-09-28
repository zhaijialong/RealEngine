#pragma once

#include "../render_graph.h"

class LightingProcessor
{
public:
    LightingProcessor(Renderer* pRenderer);
    ~LightingProcessor();

    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle velocity, uint32_t width, uint32_t height);
    
private:
    RGHandle CompositeLight(RenderGraph* pRenderGraph, RGHandle depth, RGHandle ao, RGHandle direct_lighting, 
        RGHandle indirect_specular, RGHandle indirect_diffuse, uint32_t width, uint32_t height);

    RGHandle ExtractHalfDepthNormal(RenderGraph* pRenderGraph, RGHandle depth, RGHandle normal, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<class GTAO> m_pGTAO;
    eastl::unique_ptr<class RTShadow> m_pRTShdow;
    eastl::unique_ptr<class HybridStochasticReflection> m_pReflection;
    eastl::unique_ptr<class ReSTIRGI> m_pReSTIRGI;
    eastl::unique_ptr<class ClusteredShading> m_pClusteredShading;
};