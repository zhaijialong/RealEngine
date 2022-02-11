#pragma once

#include "gtao.h"
#include "ray_traced_shadow.h"
#include "clustered_shading.h"

struct LightingProcessInput
{
    RenderGraphHandle diffuseRT;
    RenderGraphHandle specularRT;
    RenderGraphHandle normalRT;
    RenderGraphHandle emissiveRT;
    RenderGraphHandle depthRT;
};

class LightingProcessor
{
public:
    LightingProcessor(Renderer* pRenderer);

    RenderGraphHandle Process(RenderGraph* pRenderGraph, const LightingProcessInput& input, uint32_t width, uint32_t height);
    
private:
    RenderGraphHandle CompositeLight(RenderGraph* pRenderGraph, const LightingProcessInput& input, RenderGraphHandle ao, RenderGraphHandle shadow, uint32_t width, uint32_t height);

private:
    IGfxPipelineState* m_pCompositePSO = nullptr;

    std::unique_ptr<GTAO> m_pGTAO;
    std::unique_ptr<RTShadow> m_pRTShdow;
    std::unique_ptr<ClusteredShading> m_pClusteredShading;
};