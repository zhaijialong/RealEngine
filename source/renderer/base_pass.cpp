#include "base_pass.h"
#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "utils/profiler.h"

struct FirstPhaseInstanceCullingData
{
    RenderGraphHandle objectListBuffer;
    RenderGraphHandle visibleObjectListBuffer;
    RenderGraphHandle culledObjectListBuffer;
};

struct SecondPhaseInstanceCullingData
{
    RenderGraphHandle objectListBuffer;
    RenderGraphHandle visibleObjectListBuffer;
};

struct BasePassData
{
    RenderGraphHandle indirectCommandBuffer;

    RenderGraphHandle inHZB;
    RenderGraphHandle meshletListBuffer;
    RenderGraphHandle meshletListCounterBuffer;
    RenderGraphHandle occlusionCulledMeshletsBuffer; //only used in first phase
    RenderGraphHandle occlusionCulledMeshletsCounterBuffer; //only used in first phase

    RenderGraphHandle outDiffuseRT;  //srgb : diffuse(xyz) + ao(a)
    RenderGraphHandle outSpecularRT; //srgb : specular(xyz), a: not used
    RenderGraphHandle outNormalRT;   //rgba8norm : normal(xyz) + roughness(a)
    RenderGraphHandle outEmissiveRT; //r11g11b10 : emissive
    RenderGraphHandle outDepthRT;
};

static inline uint32_t roundup(uint32_t a, uint32_t b)
{
    return (a / b + 1) * b;
}

BasePass::BasePass(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "instance_culling", "cs_6_6", { "FIRST_PHASE=1" });
    m_p1stPhaseInstanceCullingPSO = pRenderer->GetPipelineState(desc, "1st phase instance culling PSO");

    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "instance_culling", "cs_6_6", { });
    m_p2ndPhaseInstanceCullingPSO = pRenderer->GetPipelineState(desc, "2nd phase instance culling PSO");

    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "build_meshlet_list", "cs_6_6", { });
    m_pBuildMeshletListPSO = pRenderer->GetPipelineState(desc, "build meshlet list PSO");

    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "build_instance_culling_command", "cs_6_6", { });
    m_pBuildInstanceCullingCommandPSO = pRenderer->GetPipelineState(desc, "indirect instance culling command PSO");

    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "build_indirect_command", "cs_6_6", {});
    m_pBuildIndirectCommandPSO = pRenderer->GetPipelineState(desc, "indirect command PSO");
}

RenderBatch& BasePass::AddBatch()
{
    LinearAllocator* allocator = m_pRenderer->GetConstantAllocator();
    return m_instances.emplace_back(*allocator);
}

void BasePass::Render1stPhase(RenderGraph* pRenderGraph)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "BasePass 1st phase");

    MergeBatches();

    uint32_t max_dispatch_num = roundup((uint32_t)m_indirectBatches.size(), 65536 / sizeof(uint32_t));
    uint32_t max_instance_num = roundup(m_pRenderer->GetInstanceCount(), 65536 / sizeof(uint8_t));
    uint32_t max_meshlets_num = roundup(m_nTotalMeshletCount, 65536 / sizeof(uint2));

    HZB* pHZB = m_pRenderer->GetHZB();

    struct ClearCounterPassData
    {
        RenderGraphHandle firstPhaseMeshletListCounterBuffer;
        RenderGraphHandle secondPhaseObjectListCounterBuffer;
        RenderGraphHandle secondPhaseMeshletListCounterBuffer;
    };
    auto clear_counter_pass = pRenderGraph->AddPass<ClearCounterPassData>("Clear Counter",
        [&](ClearCounterPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = 4;
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.format = GfxFormat::R32UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer | GfxBufferUsageUnorderedAccess;

            data.firstPhaseMeshletListCounterBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "1st phase meshlet list counter");
            data.firstPhaseMeshletListCounterBuffer = builder.Write(data.firstPhaseMeshletListCounterBuffer, GfxResourceState::UnorderedAccess);

            data.secondPhaseObjectListCounterBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd phase object list counter");
            data.secondPhaseObjectListCounterBuffer = builder.Write(data.secondPhaseObjectListCounterBuffer, GfxResourceState::UnorderedAccess);

            data.secondPhaseMeshletListCounterBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd phase meshlet list counter");
            data.secondPhaseMeshletListCounterBuffer = builder.Write(data.secondPhaseMeshletListCounterBuffer, GfxResourceState::UnorderedAccess);
        },
        [=](const ClearCounterPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* firstPhaseMeshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.firstPhaseMeshletListCounterBuffer);
            RenderGraphBuffer* secondPhaseObjectListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.secondPhaseObjectListCounterBuffer);
            RenderGraphBuffer* secondPhaseMeshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.secondPhaseMeshletListCounterBuffer);

            ResetCounter(pCommandList, firstPhaseMeshletListCounterBuffer, secondPhaseObjectListCounterBuffer, secondPhaseMeshletListCounterBuffer);
        });

    struct InstanceCullingData
    {
        RenderGraphHandle hzbTexture;
        RenderGraphHandle cullingResultBuffer; //[true, false, false, ...] for each instance
        RenderGraphHandle secondPhaseObjectListBuffer;
        RenderGraphHandle secondPhaseObjectListCounterBuffer;
    };
    auto instance_culling_pass = pRenderGraph->AddPass<InstanceCullingData>("Instance Culling",
        [&](InstanceCullingData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = 1;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R8UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer | GfxBufferUsageUnorderedAccess;
            data.cullingResultBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "1st phase culling result");
            data.cullingResultBuffer = builder.Write(data.cullingResultBuffer, GfxResourceState::UnorderedAccess);

            bufferDesc.stride = 4;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R32UI;
            data.secondPhaseObjectListBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd phase object list");
            data.secondPhaseObjectListBuffer = builder.Write(data.secondPhaseObjectListBuffer, GfxResourceState::UnorderedAccess);

            data.secondPhaseObjectListCounterBuffer = builder.Write(clear_counter_pass->secondPhaseObjectListCounterBuffer, GfxResourceState::UnorderedAccess);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.hzbTexture = builder.Read(pHZB->Get1stPhaseCullingHZBMip(i), GfxResourceState::ShaderResourceNonPS, i);
            }
        },
        [=](const InstanceCullingData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* cullingResultBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.cullingResultBuffer);
            RenderGraphBuffer* secondPhaseObjectListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.secondPhaseObjectListBuffer);
            RenderGraphBuffer* secondPhaseObjectListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.secondPhaseObjectListCounterBuffer);

            uint32_t clear_value[4] = { 0, 0, 0, 0 };
            pCommandList->ClearUAV(cullingResultBuffer->GetBuffer(), cullingResultBuffer->GetUAV(), clear_value);
            pCommandList->UavBarrier(cullingResultBuffer->GetBuffer());

            InstanceCulling1stPhase(pCommandList, cullingResultBuffer->GetUAV(), secondPhaseObjectListBuffer->GetUAV(), secondPhaseObjectListCounterBuffer->GetUAV());
        });

    struct BuildMeshletListData
    {
        RenderGraphHandle cullingResultBuffer;
        RenderGraphHandle meshletListBuffer;
        RenderGraphHandle meshletListCounterBuffer;
    };

    auto build_meshlet_list_pass = pRenderGraph->AddPass<BuildMeshletListData>("Build Meshlet List",
        [&](BuildMeshletListData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint2);
            bufferDesc.size = bufferDesc.stride * max_meshlets_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.meshletListBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "1st phase meshlet list");

            data.cullingResultBuffer = builder.Read(instance_culling_pass->cullingResultBuffer, GfxResourceState::ShaderResourceNonPS);
            data.meshletListBuffer = builder.Write(data.meshletListBuffer, GfxResourceState::UnorderedAccess);
            data.meshletListCounterBuffer = builder.Write(clear_counter_pass->firstPhaseMeshletListCounterBuffer, GfxResourceState::UnorderedAccess);
        },
        [=](const BuildMeshletListData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* cullingResultBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.cullingResultBuffer);
            RenderGraphBuffer* meshletListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListBuffer);
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);

            BuildMeshletList(pCommandList, cullingResultBuffer->GetSRV(), meshletListBuffer->GetUAV(), meshletListCounterBuffer->GetUAV());
        });

    struct BuildIndirectCommandPassData
    {
        RenderGraphHandle meshletListCounterBuffer;
        RenderGraphHandle indirectCommandBuffer;
    };

    auto build_indirect_command = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command",
        [&](BuildIndirectCommandPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint3);
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.indirectCommandBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "1st Phase Indirect Command");
            data.indirectCommandBuffer = builder.Write(data.indirectCommandBuffer, GfxResourceState::UnorderedAccess);

            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, GfxResourceState::ShaderResourceNonPS);
        },
        [=](const BuildIndirectCommandPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);
            RenderGraphBuffer* commandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            BuildIndirectCommand(pCommandList, meshletListCounterBuffer->GetSRV(), commandBuffer->GetUAV());
        });

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass",
        [&](BasePassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_pRenderer->GetBackbufferWidth();
            desc.height = m_pRenderer->GetBackbufferHeight();
            desc.usage = GfxTextureUsageRenderTarget;
            desc.format = GfxFormat::RGBA8SRGB;
            data.outDiffuseRT = builder.Create<RenderGraphTexture>(desc, "Diffuse RT");
            data.outSpecularRT = builder.Create<RenderGraphTexture>(desc, "Specular RT");

            desc.format = GfxFormat::RGBA8UNORM;
            data.outNormalRT = builder.Create<RenderGraphTexture>(desc, "Normal RT");

            desc.format = GfxFormat::R11G11B10F;
            data.outEmissiveRT = builder.Create<RenderGraphTexture>(desc, "Emissive RT");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil;
            data.outDepthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth RT");

            data.outDiffuseRT = builder.WriteColor(0, data.outDiffuseRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outSpecularRT = builder.WriteColor(1, data.outSpecularRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outNormalRT = builder.WriteColor(2, data.outNormalRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outEmissiveRT = builder.WriteColor(3, data.outEmissiveRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outDepthRT = builder.WriteDepth(data.outDepthRT, 0, GfxRenderPassLoadOp::Clear, GfxRenderPassLoadOp::Clear);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.inHZB = builder.Read(pHZB->Get1stPhaseCullingHZBMip(i), GfxResourceState::ShaderResourceNonPS, i);
            }

            data.indirectCommandBuffer = builder.Read(build_indirect_command->indirectCommandBuffer, GfxResourceState::IndirectArg);
            data.meshletListBuffer = builder.Read(build_meshlet_list_pass->meshletListBuffer, GfxResourceState::ShaderResourceNonPS);
            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, GfxResourceState::ShaderResourceNonPS);

            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint2);
            bufferDesc.size = bufferDesc.stride * max_meshlets_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.occlusionCulledMeshletsBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd phase meshlet list");
            data.occlusionCulledMeshletsBuffer = builder.Write(data.occlusionCulledMeshletsBuffer, GfxResourceState::UnorderedAccess);

            data.occlusionCulledMeshletsCounterBuffer = builder.Write(clear_counter_pass->secondPhaseMeshletListCounterBuffer, GfxResourceState::UnorderedAccess);
        },
        [=](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* commandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            RenderGraphBuffer* meshletListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListBuffer);
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);

            Flush1stPhaseBatches(pCommandList, commandBuffer->GetBuffer(), meshletListBuffer->GetSRV(), meshletListCounterBuffer->GetSRV());
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;

    m_secondPhaseObjectListBuffer = instance_culling_pass->secondPhaseObjectListBuffer;
    m_secondPhaseObjectListCounterBuffer = instance_culling_pass->secondPhaseObjectListCounterBuffer;

    m_secondPhaseMeshletListBuffer = gbuffer_pass->occlusionCulledMeshletsBuffer;
    m_secondPhaseMeshletListCounterBuffer = gbuffer_pass->occlusionCulledMeshletsCounterBuffer;
}

void BasePass::Render2ndPhase(RenderGraph* pRenderGraph)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "BasePass 2nd phase");

    HZB* pHZB = m_pRenderer->GetHZB();

    uint32_t max_dispatch_num = roundup((uint32_t)m_indirectBatches.size(), 65536 / sizeof(uint32_t));
    uint32_t max_instance_num = roundup(m_pRenderer->GetInstanceCount(), 65536 / sizeof(uint8_t));
    uint32_t max_meshlets_num = roundup(m_nTotalMeshletCount, 65536 / sizeof(uint2));

    struct BuildCullingCommandData
    {
        RenderGraphHandle objectListCounterBuffer;
        RenderGraphHandle commandBuffer;
    };
    auto build_culling_command = pRenderGraph->AddPass<BuildCullingCommandData>("Build Instance Culling Command",
        [&](BuildCullingCommandData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc desc;
            desc.stride = sizeof(uint3);
            desc.size = desc.stride;
            desc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.commandBuffer = builder.Create<RenderGraphBuffer>(desc, "2nd phase instance culling command");
            data.commandBuffer = builder.Write(data.commandBuffer, GfxResourceState::UnorderedAccess);

            data.objectListCounterBuffer = builder.Read(m_secondPhaseObjectListCounterBuffer, GfxResourceState::ShaderResourceNonPS);
        },
        [=](const BuildCullingCommandData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* commandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.commandBuffer);
            RenderGraphBuffer* objectListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.objectListCounterBuffer);

            pCommandList->SetPipelineState(m_pBuildInstanceCullingCommandPSO);

            uint32_t consts[2] = { commandBuffer->GetUAV()->GetHeapIndex(), objectListCounterBuffer->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, consts, sizeof(consts));
            pCommandList->Dispatch(1, 1, 1);
        });

    struct InstanceCullingData
    {
        RenderGraphHandle indirectCommandBuffer;
        RenderGraphHandle hzbTexture;
        RenderGraphHandle cullingResultBuffer; //[true, false, false, ...] for each instance
        RenderGraphHandle objectListBuffer;
        RenderGraphHandle objectListCounterBuffer;
    };

    auto instance_culling_pass = pRenderGraph->AddPass<InstanceCullingData>("Instance Culling",
        [&](InstanceCullingData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = 1;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R8UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer | GfxBufferUsageUnorderedAccess;
            data.cullingResultBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd phase culling result");
            data.cullingResultBuffer = builder.Write(data.cullingResultBuffer, GfxResourceState::UnorderedAccess);

            data.indirectCommandBuffer = builder.Read(build_culling_command->commandBuffer, GfxResourceState::IndirectArg);
            data.objectListBuffer = builder.Read(m_secondPhaseObjectListBuffer, GfxResourceState::ShaderResourceNonPS);
            data.objectListCounterBuffer = builder.Read(m_secondPhaseObjectListCounterBuffer, GfxResourceState::ShaderResourceNonPS);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.hzbTexture = builder.Read(pHZB->Get2ndPhaseCullingHZBMip(i), GfxResourceState::ShaderResourceNonPS, i);
            }
        },
        [=](const InstanceCullingData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* indirectCommandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            RenderGraphBuffer* cullingResultBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.cullingResultBuffer);
            RenderGraphBuffer* objectListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.objectListBuffer);
            RenderGraphBuffer* objectListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.objectListCounterBuffer);
            
            uint32_t clear_value[4] = { 0, 0, 0, 0 };
            pCommandList->ClearUAV(cullingResultBuffer->GetBuffer(), cullingResultBuffer->GetUAV(), clear_value);
            pCommandList->UavBarrier(cullingResultBuffer->GetBuffer());

            InstanceCulling2ndPhase(pCommandList, indirectCommandBuffer->GetBuffer(), cullingResultBuffer->GetUAV(), objectListBuffer->GetSRV(), objectListCounterBuffer->GetSRV());
        });

    struct BuildMeshletListData
    {
        RenderGraphHandle cullingResultBuffer;
        RenderGraphHandle meshletListBuffer;
        RenderGraphHandle meshletListCounterBuffer;
    };

    auto build_meshlet_list_pass = pRenderGraph->AddPass<BuildMeshletListData>("Build Meshlet List",
        [&](BuildMeshletListData& data, RenderGraphBuilder& builder)
        {
            data.cullingResultBuffer = builder.Read(instance_culling_pass->cullingResultBuffer, GfxResourceState::ShaderResourceNonPS);
            data.meshletListBuffer = builder.Write(m_secondPhaseMeshletListBuffer, GfxResourceState::UnorderedAccess);
            data.meshletListCounterBuffer = builder.Write(m_secondPhaseMeshletListCounterBuffer, GfxResourceState::UnorderedAccess);
        },
        [=](const BuildMeshletListData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* cullingResultBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.cullingResultBuffer);
            RenderGraphBuffer* meshletListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListBuffer);
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);

            BuildMeshletList(pCommandList, cullingResultBuffer->GetSRV(), meshletListBuffer->GetUAV(), meshletListCounterBuffer->GetUAV());
        });

    struct BuildIndirectCommandPassData
    {
        RenderGraphHandle meshletListCounterBuffer;
        RenderGraphHandle indirectCommandBuffer;
    };

    auto build_indirect_command = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command",
        [&](BuildIndirectCommandPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint3);
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.indirectCommandBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd Phase Indirect Command");
            data.indirectCommandBuffer = builder.Write(data.indirectCommandBuffer, GfxResourceState::UnorderedAccess);

            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, GfxResourceState::ShaderResourceNonPS);
        },
        [=](const BuildIndirectCommandPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);
            RenderGraphBuffer* commandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            BuildIndirectCommand(pCommandList, meshletListCounterBuffer->GetSRV(), commandBuffer->GetUAV());
        });

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass",
        [&](BasePassData& data, RenderGraphBuilder& builder)
        {
            data.outDiffuseRT = builder.WriteColor(0, m_diffuseRT, 0, GfxRenderPassLoadOp::Load);
            data.outSpecularRT = builder.WriteColor(1, m_specularRT, 0, GfxRenderPassLoadOp::Load);
            data.outNormalRT = builder.WriteColor(2, m_normalRT, 0, GfxRenderPassLoadOp::Load);
            data.outEmissiveRT = builder.WriteColor(3, m_emissiveRT, 0, GfxRenderPassLoadOp::Load);
            data.outDepthRT = builder.WriteDepth(m_depthRT, 0, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.inHZB = builder.Read(pHZB->Get2ndPhaseCullingHZBMip(i), GfxResourceState::ShaderResourceNonPS, i);
            }

            data.meshletListBuffer = builder.Read(build_meshlet_list_pass->meshletListBuffer, GfxResourceState::ShaderResourceNonPS);
            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, GfxResourceState::ShaderResourceNonPS);
            data.indirectCommandBuffer = builder.Read(build_indirect_command->indirectCommandBuffer, GfxResourceState::IndirectArg);
        },
        [=](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* indirectCommandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            RenderGraphBuffer* meshletListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListBuffer);
            RenderGraphBuffer* meshletListCounterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.meshletListCounterBuffer);

            Flush2ndPhaseBatches(pCommandList, indirectCommandBuffer->GetBuffer(), meshletListBuffer->GetSRV(), meshletListCounterBuffer->GetSRV());
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;
}

void BasePass::MergeBatches()
{
    m_nTotalInstanceCount = (uint32_t)m_instances.size();

    std::vector<uint32_t> instanceIndices(m_nTotalInstanceCount);
    for (uint32_t i = 0; i < m_nTotalInstanceCount; ++i)
    {
        instanceIndices[i] = m_instances[i].instanceIndex;
    }
    m_instanceIndexAddress = m_pRenderer->AllocateSceneConstant(instanceIndices.data(), sizeof(uint32_t) * m_nTotalInstanceCount);

    m_nTotalMeshletCount = 0;
    m_indirectBatches.clear();
    m_nonGpuDrivenBatches.clear();

    struct MergedBatch
    {
        std::vector<RenderBatch> batches;
        uint32_t meshletCount;
    };
    std::map<IGfxPipelineState*, MergedBatch> mergedBatches;

    for (size_t i = 0; i < m_instances.size(); ++i)
    {
        const RenderBatch& batch = m_instances[i];
        if (batch.pso->GetType() == GfxPipelineType::MeshShading)
        {
            m_nTotalMeshletCount += batch.meshletCount;

            auto iter = mergedBatches.find(batch.pso);
            if (iter != mergedBatches.end())
            {
                iter->second.meshletCount += batch.meshletCount;
                iter->second.batches.push_back(batch);
            }
            else
            {
                MergedBatch mergedBatch;
                mergedBatch.batches.push_back(batch);
                mergedBatch.meshletCount = batch.meshletCount;
                mergedBatches.insert(std::make_pair(batch.pso, mergedBatch));
            }
        }
        else
        {
            m_nonGpuDrivenBatches.push_back(batch);
        }
    }

    uint32_t meshletListOffset = 0;
    for (auto iter = mergedBatches.begin(); iter != mergedBatches.end(); ++iter)
    {
        const MergedBatch& batch = iter->second;

        std::vector<uint2> meshletList;
        meshletList.reserve(batch.meshletCount);

        for (size_t i = 0; i < batch.batches.size(); ++i)
        {
            uint32_t instanceIndex = batch.batches[i].instanceIndex;
            for (size_t m = 0; m < batch.batches[i].meshletCount; ++m)
            {
                meshletList.emplace_back(instanceIndex, (uint32_t)m);
            }
        }

        uint32_t meshletListAddress = m_pRenderer->AllocateSceneConstant(meshletList.data(), sizeof(uint2) * (uint32_t)meshletList.size());

        m_indirectBatches.push_back({ iter->first, meshletListAddress, batch.meshletCount, meshletListOffset });

        meshletListOffset += batch.meshletCount;
    }

    m_instances.clear();
}

void BasePass::ResetCounter(IGfxCommandList* pCommandList, RenderGraphBuffer* firstPhaseMeshletCounter, RenderGraphBuffer* secondPhaseObjectCounter, RenderGraphBuffer* secondPhaseMeshletCounter)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(firstPhaseMeshletCounter->GetBuffer(), firstPhaseMeshletCounter->GetUAV(), clear_value);
    pCommandList->ClearUAV(secondPhaseObjectCounter->GetBuffer(), secondPhaseObjectCounter->GetUAV(), clear_value);
    pCommandList->ClearUAV(secondPhaseMeshletCounter->GetBuffer(), secondPhaseMeshletCounter->GetUAV(), clear_value);

    pCommandList->UavBarrier(firstPhaseMeshletCounter->GetBuffer());
    pCommandList->UavBarrier(secondPhaseObjectCounter->GetBuffer());
    pCommandList->UavBarrier(secondPhaseMeshletCounter->GetBuffer());
}

void BasePass::InstanceCulling1stPhase(IGfxCommandList* pCommandList, IGfxDescriptor* cullingResultUAV, IGfxDescriptor* secondPhaseObjectListUAV, IGfxDescriptor* secondPhaseObjectListCounterUAV)
{
    pCommandList->SetPipelineState(m_p1stPhaseInstanceCullingPSO);

    uint32_t instance_count = m_nTotalInstanceCount;
    uint32_t root_consts[5] = { m_instanceIndexAddress, instance_count, cullingResultUAV->GetHeapIndex(), secondPhaseObjectListUAV->GetHeapIndex(), secondPhaseObjectListCounterUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    uint32_t group_count = max((instance_count + 63) / 64, 1u); //avoid empty dispatch warning
    pCommandList->Dispatch(group_count, 1, 1);
}

void BasePass::InstanceCulling2ndPhase(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* cullingResultUAV, IGfxDescriptor* objectListBufferSRV, IGfxDescriptor* objectListCounterBufferSRV)
{
    pCommandList->SetPipelineState(m_p2ndPhaseInstanceCullingPSO);

    uint32_t root_consts[3] = { objectListBufferSRV->GetHeapIndex(), objectListCounterBufferSRV->GetHeapIndex(), cullingResultUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->DispatchIndirect(pIndirectCommandBuffer, 0);
}

void BasePass::Flush1stPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* pMeshletListSRV, IGfxDescriptor* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.pso);

        uint32_t root_consts[5] = { pMeshletListSRV->GetHeapIndex(), pMeshletListCounterSRV->GetHeapIndex(), batch.meshletListBufferOffset, (uint32_t)i, 1};
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer, sizeof(uint3) * (uint32_t)i);
    }
}

void BasePass::Flush2ndPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer, IGfxDescriptor* pMeshletListSRV, IGfxDescriptor* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.pso);

        uint32_t root_consts[5] = { pMeshletListSRV->GetHeapIndex(), pMeshletListCounterSRV->GetHeapIndex(), batch.meshletListBufferOffset, (uint32_t)i, 0 };
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer, sizeof(uint3) * (uint32_t)i);
    }

    for (size_t i = 0; i < m_nonGpuDrivenBatches.size(); ++i)
    {
        DrawBatch(pCommandList, m_nonGpuDrivenBatches[i]);
    }
}

void BasePass::BuildMeshletList(IGfxCommandList* pCommandList, IGfxDescriptor* cullingResultSRV, IGfxDescriptor* meshletListBufferUAV, IGfxDescriptor* meshletListCounterBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildMeshletListPSO);

    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        uint32_t consts[7] = {
            (uint32_t)i,
            cullingResultSRV->GetHeapIndex(),
            m_indirectBatches[i].originMeshletListAddress,
            m_indirectBatches[i].originMeshletCount,
            m_indirectBatches[i].meshletListBufferOffset,
            meshletListBufferUAV->GetHeapIndex(),
            meshletListCounterBufferUAV->GetHeapIndex() };
        pCommandList->SetComputeConstants(1, consts, sizeof(consts));
        pCommandList->Dispatch((m_indirectBatches[i].originMeshletCount + 63) / 64, 1, 1);
    }
}

void BasePass::BuildIndirectCommand(IGfxCommandList* pCommandList, IGfxDescriptor* pCounterBufferSRV, IGfxDescriptor* pCommandBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildIndirectCommandPSO);

    uint32_t batch_count = (uint32_t)m_indirectBatches.size();

    uint32_t consts[3] = { batch_count, pCounterBufferSRV->GetHeapIndex(), pCommandBufferUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, consts, sizeof(consts));

    uint32_t group_count = max((batch_count + 63) / 64, 1u); //avoid empty dispatch warning
    pCommandList->Dispatch(group_count, 1, 1);
}
