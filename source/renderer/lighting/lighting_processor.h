#pragma once

#include "gtao.h"
#include "clustered_shading.h"

struct LightingProcessInput
{
    RenderGraphHandle albedoRT;
    RenderGraphHandle normalRT;
    RenderGraphHandle emissiveRT;
    RenderGraphHandle depthRT;
    RenderGraphHandle shadowRT;
};

class LightingProcessor
{
public:
    LightingProcessor(Renderer* pRenderer);

    RenderGraphHandle Process(RenderGraph* pRenderGraph, const LightingProcessInput& input, uint32_t width, uint32_t height);
    
private:
    std::unique_ptr<GTAO> m_pGTAO;
    std::unique_ptr<ClusteredShading> m_pClusteredShading;
};