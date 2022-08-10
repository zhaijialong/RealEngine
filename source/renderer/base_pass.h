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

    RGHandle GetDiffuseRT() const { return m_diffuseRT; }
    RGHandle GetSpecularRT() const { return m_specularRT; }
    RGHandle GetNormalRT() const { return m_normalRT; }
    RGHandle GetEmissiveRT() const { return m_emissiveRT; }
    RGHandle GetCustomDataRT() const { return m_customDataRT; }
    RGHandle GetDepthRT() const { return m_depthRT; }

    RGHandle GetSecondPhaseMeshletListBuffer() const { return m_secondPhaseMeshletListBuffer; }
    RGHandle GetSecondPhaseMeshletListCounterBuffer() const { return m_secondPhaseMeshletListCounterBuffer; }

private:
    void MergeBatches();

    void ResetCounter(IGfxCommandList* pCommandList, RGBuffer* firstPhaseMeshletCounter, RGBuffer* secondPhaseObjectCounter, RGBuffer* secondPhaseMeshletCounter);
    void InstanceCulling1stPhase(IGfxCommandList* pCommandList, RGBuffer* cullingResultUAV, RGBuffer* secondPhaseObjectListUAV, RGBuffer* secondPhaseObjectListCounterUAV);
    void InstanceCulling2ndPhase(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* cullingResultUAV, RGBuffer* objectListBufferSRV, RGBuffer* objectListCounterBufferSRV);

    void Flush1stPhaseBatches(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV);
    void Flush2ndPhaseBatches(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV);

    void BuildMeshletList(IGfxCommandList* pCommandList, RGBuffer* cullingResultSRV, RGBuffer* meshletListBufferUAV, RGBuffer* meshletListCounterBufferUAV);
    void BuildIndirectCommand(IGfxCommandList* pCommandList, RGBuffer* pCounterBufferSRV, RGBuffer* pCommandBufferUAV);
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

    RGHandle m_diffuseRT;
    RGHandle m_specularRT;
    RGHandle m_normalRT;
    RGHandle m_emissiveRT;
    RGHandle m_customDataRT;
    RGHandle m_depthRT;

    RGHandle m_secondPhaseObjectListBuffer;
    RGHandle m_secondPhaseObjectListCounterBuffer;

    RGHandle m_secondPhaseMeshletListBuffer;
    RGHandle m_secondPhaseMeshletListCounterBuffer;
};