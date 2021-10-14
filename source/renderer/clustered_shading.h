#pragma once

#include "render_graph.h"

struct ClusterShadingPassData
{
    RenderGraphHandle albedoRT;
    RenderGraphHandle normalRT;
    RenderGraphHandle emissiveRT;
    RenderGraphHandle depthRT;
    RenderGraphHandle shadowRT;

    RenderGraphHandle hdrRT;
};

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, const ClusterShadingPassData& data, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};