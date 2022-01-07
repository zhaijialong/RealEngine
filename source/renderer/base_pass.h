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

    RenderGraphHandle GetOcclusionCulledMeshletsBuffer() const { return m_occlusionCulledMeshletsBuffer; }
    RenderGraphHandle GetOcclusionCulledMeshletsCounterBuffer() const { return m_occlusionCulledMeshletsCounterBuffer; }

private:
    void MergeBatches();
    void Flush1stPhaseBatches(IGfxCommandList* pCommandList);
    void Flush2ndPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer);

    void BuildIndirectCommand(IGfxCommandList* pCommandList, IGfxDescriptor* pCounterBufferSRV, IGfxDescriptor* pCommandBufferUAV);
private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pBuildIndirectCommandPSO = nullptr;

    std::vector<RenderBatch> m_batches;

    struct MergedBatch
    {
        std::vector<RenderBatch> batches;
        uint32_t meshletCount;
    };
    std::map<IGfxPipelineState*, MergedBatch> m_mergedBatches;

    struct IndirectBatch
    {
        IGfxPipelineState* pso;
        uint32_t occlusionCulledMeshletsBufferOffset;
    };
    std::vector<IndirectBatch> m_indirectBatches;

    std::vector<RenderBatch> m_nonMergedBatches;

    uint32_t m_nTotalMeshletCount = 0;

    RenderGraphHandle m_diffuseRT;
    RenderGraphHandle m_specularRT;
    RenderGraphHandle m_normalRT;
    RenderGraphHandle m_emissiveRT;
    RenderGraphHandle m_depthRT;

    RenderGraphHandle m_occlusionCulledMeshletsBuffer;
    RenderGraphHandle m_occlusionCulledMeshletsCounterBuffer;
};