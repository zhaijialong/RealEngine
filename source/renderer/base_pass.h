#pragma once

#include "render_batch.h"
#include "render_graph.h"
#include <map>

class Renderer;

class BasePass
{
public:
    BasePass(Renderer* pRenderer);

    RenderBatch& AddBatch();
    void Render1stPhase(RenderGraph* pRenderGraph);
    void Render2ndPhase(RenderGraph* pRenderGraph);

    RenderGraphHandle GetDiffuseRT() const { return m_diffuseRT; }
    RenderGraphHandle GetSpecularRT() const { return m_specularRT; }
    RenderGraphHandle GetNormalRT() const { return m_normalRT; }
    RenderGraphHandle GetEmissiveRT() const { return m_emissiveRT; }
    RenderGraphHandle GetDepthRT() const { return m_depthRT; }

private:
    void MergeBatches();
    void Flush1stPhaseBatches(IGfxCommandList* pCommandList);
    void Flush2ndPhaseBatches(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
    std::vector<RenderBatch> m_batches;

    struct MergedBatch
    {
        std::vector<RenderBatch> batches;
        uint32_t meshletCount;
    };
    std::map<IGfxPipelineState*, MergedBatch> m_mergedBatches;
    std::vector<RenderBatch> m_nonMergedBatches;

    RenderGraphHandle m_diffuseRT;
    RenderGraphHandle m_specularRT;
    RenderGraphHandle m_normalRT;
    RenderGraphHandle m_emissiveRT;
    RenderGraphHandle m_depthRT;
};