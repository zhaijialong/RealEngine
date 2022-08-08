#pragma once

#include "render_batch.h"
#include "render_graph.h"

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
    RenderGraphHandle GetCustomDataRT() const { return m_customDataRT; }
    RenderGraphHandle GetDepthRT() const { return m_depthRT; }

    RenderGraphHandle GetSecondPhaseMeshletListBuffer() const { return m_secondPhaseMeshletListBuffer; }
    RenderGraphHandle GetSecondPhaseMeshletListCounterBuffer() const { return m_secondPhaseMeshletListCounterBuffer; }

private:
    void MergeBatches();

    void ResetCounter(IGfxCommandList* pCommandList, RenderGraphBuffer* firstPhaseMeshletCounter, RenderGraphBuffer* secondPhaseObjectCounter, RenderGraphBuffer* secondPhaseMeshletCounter);
    void InstanceCulling1stPhase(IGfxCommandList* pCommandList, RenderGraphBuffer* cullingResultUAV, RenderGraphBuffer* secondPhaseObjectListUAV, RenderGraphBuffer* secondPhaseObjectListCounterUAV);
    void InstanceCulling2ndPhase(IGfxCommandList* pCommandList, RenderGraphBuffer* pIndirectCommandBuffer, RenderGraphBuffer* cullingResultUAV, RenderGraphBuffer* objectListBufferSRV, RenderGraphBuffer* objectListCounterBufferSRV);

    void Flush1stPhaseBatches(IGfxCommandList* pCommandList, RenderGraphBuffer* pIndirectCommandBuffer, RenderGraphBuffer* pMeshletListSRV, RenderGraphBuffer* pMeshletListCounterSRV);
    void Flush2ndPhaseBatches(IGfxCommandList* pCommandList, RenderGraphBuffer* pIndirectCommandBuffer, RenderGraphBuffer* pMeshletListSRV, RenderGraphBuffer* pMeshletListCounterSRV);

    void BuildMeshletList(IGfxCommandList* pCommandList, RenderGraphBuffer* cullingResultSRV, RenderGraphBuffer* meshletListBufferUAV, RenderGraphBuffer* meshletListCounterBufferUAV);
    void BuildIndirectCommand(IGfxCommandList* pCommandList, RenderGraphBuffer* pCounterBufferSRV, RenderGraphBuffer* pCommandBufferUAV);
private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_p1stPhaseInstanceCullingPSO = nullptr;
    IGfxPipelineState* m_p2ndPhaseInstanceCullingPSO = nullptr;

    IGfxPipelineState* m_pBuildMeshletListPSO = nullptr;
    IGfxPipelineState* m_pBuildInstanceCullingCommandPSO = nullptr;
    IGfxPipelineState* m_pBuildIndirectCommandPSO = nullptr;

    eastl::vector<RenderBatch> m_instances;

    struct IndirectBatch
    {
        IGfxPipelineState* pso;
        uint32_t originMeshletListAddress;
        uint32_t originMeshletCount;
        uint32_t meshletListBufferOffset;
    };
    eastl::vector<IndirectBatch> m_indirectBatches;

    eastl::vector<RenderBatch> m_nonGpuDrivenBatches;

    uint32_t m_nTotalInstanceCount = 0;
    uint32_t m_nTotalMeshletCount = 0;


    uint32_t m_instanceIndexAddress = 0; //[0, 24, 27, 122, ...] size : m_nTotalInstanceCount

    RenderGraphHandle m_diffuseRT;
    RenderGraphHandle m_specularRT;
    RenderGraphHandle m_normalRT;
    RenderGraphHandle m_emissiveRT;
    RenderGraphHandle m_customDataRT;
    RenderGraphHandle m_depthRT;

    RenderGraphHandle m_secondPhaseObjectListBuffer;
    RenderGraphHandle m_secondPhaseObjectListCounterBuffer;

    RenderGraphHandle m_secondPhaseMeshletListBuffer;
    RenderGraphHandle m_secondPhaseMeshletListCounterBuffer;
};