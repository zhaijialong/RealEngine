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

    RenderGraphHandle GetSecondPhaseMeshletListBuffer() const { return m_secondPhaseMeshletListBuffer; }
    RenderGraphHandle GetSecondPhaseMeshletListCounterBuffer() const { return m_secondPhaseMeshletListCounterBuffer; }

private:
    void MergeBatches();

    void ResetCounter(IGfxCommandList* pCommandList, RenderGraphBuffer* firstPhaseMeshletCounter, RenderGraphBuffer* secondPhaseObjectCounter, RenderGraphBuffer* secondPhaseMeshletCounter);
    void InstanceCulling1stPhase(IGfxCommandList* pCommandList, IGfxDescriptor* cullingResultUAV, IGfxDescriptor* secondPhaseObjectListUAV, IGfxDescriptor* secondPhaseObjectListCounterUAV);
    void InstanceCulling2ndPhase(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* cullingResultUAV, IGfxDescriptor* objectListBufferSRV, IGfxDescriptor* objectListCounterBufferSRV);

    void Flush1stPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* pMeshletListSRV, IGfxDescriptor* pMeshletListCounterSRV);
    void Flush2ndPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* pMeshletListSRV, IGfxDescriptor* pMeshletListCounterSRV);

    void BuildMeshletList(IGfxCommandList* pCommandList, IGfxDescriptor* cullingResultSRV, IGfxDescriptor* meshletListBufferUAV, IGfxDescriptor* meshletListCounterBufferUAV);
    void BuildIndirectCommand(IGfxCommandList* pCommandList, IGfxDescriptor* pCounterBufferSRV, IGfxDescriptor* pCommandBufferUAV);
private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_p1stPhaseInstanceCullingPSO = nullptr;
    IGfxPipelineState* m_p2ndPhaseInstanceCullingPSO = nullptr;

    IGfxPipelineState* m_pBuildMeshletListPSO = nullptr;
    IGfxPipelineState* m_pBuildInstanceCullingCommandPSO = nullptr;
    IGfxPipelineState* m_pBuildIndirectCommandPSO = nullptr;

    std::vector<RenderBatch> m_instances;

    struct IndirectBatch
    {
        IGfxPipelineState* pso;
        uint32_t originMeshletListAddress;
        uint32_t originMeshletCount;
        uint32_t meshletListBufferOffset;
    };
    std::vector<IndirectBatch> m_indirectBatches;

    std::vector<RenderBatch> m_nonGpuDrivenBatches;

    uint32_t m_nTotalInstanceCount = 0;
    uint32_t m_nTotalMeshletCount = 0;


    uint32_t m_instanceIndexAddress = 0; //[0, 24, 27, 122, ...] size : m_nTotalInstanceCount

    RenderGraphHandle m_diffuseRT;
    RenderGraphHandle m_specularRT;
    RenderGraphHandle m_normalRT;
    RenderGraphHandle m_emissiveRT;
    RenderGraphHandle m_depthRT;

    RenderGraphHandle m_secondPhaseObjectListBuffer;
    RenderGraphHandle m_secondPhaseObjectListCounterBuffer;

    RenderGraphHandle m_secondPhaseMeshletListBuffer;
    RenderGraphHandle m_secondPhaseMeshletListCounterBuffer;
};