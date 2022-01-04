#pragma once

#include "render_batch.h"
#include "render_graph.h"

class Renderer;

class BasePass
{
public:
    BasePass(Renderer* pRenderer);

    RenderBatch& AddBatch();
    void Render(RenderGraph* pRenderGraph);

    RenderGraphHandle GetDiffuseRT() const { return m_diffuseRT; }
    RenderGraphHandle GetSpecularRT() const { return m_specularRT; }
    RenderGraphHandle GetNormalRT() const { return m_normalRT; }
    RenderGraphHandle GetEmissiveRT() const { return m_emissiveRT; }
    RenderGraphHandle GetDepthRT() const { return m_depthRT; }

private:
    Renderer* m_pRenderer;
    std::vector<RenderBatch> m_batchs;

    RenderGraphHandle m_diffuseRT;
    RenderGraphHandle m_specularRT;
    RenderGraphHandle m_normalRT;
    RenderGraphHandle m_emissiveRT;
    RenderGraphHandle m_depthRT;
};