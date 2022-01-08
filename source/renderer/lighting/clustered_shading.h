#pragma once

#include "../render_graph.h"

struct ClusterShadingPassData
{
    RenderGraphHandle inDiffuseRT;
    RenderGraphHandle inSpecularRT;
    RenderGraphHandle inNormalRT;
    RenderGraphHandle inEmissiveRT;
    RenderGraphHandle inDepthRT;
    RenderGraphHandle inShadowRT;
    RenderGraphHandle inAOTermRT;

    RenderGraphHandle outHdrRT;
};

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle diffuseRT, RenderGraphHandle specularRT, RenderGraphHandle normalRT, RenderGraphHandle emissiveRT,
        RenderGraphHandle depthRT, RenderGraphHandle shadowRT, RenderGraphHandle ao, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, const ClusterShadingPassData& data, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};