#include "base_pass.h"
#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "utils/profiler.h"
#include "EASTL/map.h"

struct FirstPhaseInstanceCullingData
{
    RGHandle objectListBuffer;
    RGHandle visibleObjectListBuffer;
    RGHandle culledObjectListBuffer;
};

struct SecondPhaseInstanceCullingData
{
    RGHandle objectListBuffer;
    RGHandle visibleObjectListBuffer;
};

struct BasePassData
{
    RGHandle indirectCommandBuffer;

    RGHandle inHZB;
    RGHandle meshletListBuffer;
    RGHandle meshletListCounterBuffer;
    RGHandle occlusionCulledMeshletsBuffer; //only used in first phase
    RGHandle occlusionCulledMeshletsCounterBuffer; //only used in first phase

    RGHandle outDiffuseRT;  //srgb : diffuse(xyz) + ao(a)
    RGHandle outSpecularRT; //srgb : specular(xyz) + shading model(a)
    RGHandle outNormalRT;   //rgba8norm : normal(xyz) + roughness(a)
    RGHandle outEmissiveRT; //r11g11b10 : emissive
    RGHandle outCustomRT;     //rgba8norm : custom data
    RGHandle outDepthRT;
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
        RGHandle firstPhaseMeshletListCounterBuffer;
        RGHandle secondPhaseObjectListCounterBuffer;
        RGHandle secondPhaseMeshletListCounterBuffer;
    };
    auto clear_counter_pass = pRenderGraph->AddPass<ClearCounterPassData>("Clear Counter", RenderPassType::Compute,
        [&](ClearCounterPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = 4;
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.format = GfxFormat::R32UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer;

            data.firstPhaseMeshletListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase meshlet list counter");
            data.firstPhaseMeshletListCounterBuffer = builder.Write(data.firstPhaseMeshletListCounterBuffer);

            data.secondPhaseObjectListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase object list counter");
            data.secondPhaseObjectListCounterBuffer = builder.Write(data.secondPhaseObjectListCounterBuffer);

            data.secondPhaseMeshletListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase meshlet list counter");
            data.secondPhaseMeshletListCounterBuffer = builder.Write(data.secondPhaseMeshletListCounterBuffer);
        },
        [=](const ClearCounterPassData& data, IGfxCommandList* pCommandList)
        {
            ResetCounter(pCommandList, 
                pRenderGraph->GetBuffer(data.firstPhaseMeshletListCounterBuffer), 
                pRenderGraph->GetBuffer(data.secondPhaseObjectListCounterBuffer),
                pRenderGraph->GetBuffer(data.secondPhaseMeshletListCounterBuffer));
        });

    struct InstanceCullingData
    {
        RGHandle hzbTexture;
        RGHandle cullingResultBuffer; //[true, false, false, ...] for each instance
        RGHandle secondPhaseObjectListBuffer;
        RGHandle secondPhaseObjectListCounterBuffer;
    };
    auto instance_culling_pass = pRenderGraph->AddPass<InstanceCullingData>("Instance Culling", RenderPassType::Compute,
        [&](InstanceCullingData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = 1;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R8UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer;
            data.cullingResultBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase culling result");
            data.cullingResultBuffer = builder.Write(data.cullingResultBuffer);

            bufferDesc.stride = 4;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R32UI;
            data.secondPhaseObjectListBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase object list");
            data.secondPhaseObjectListBuffer = builder.Write(data.secondPhaseObjectListBuffer);

            data.secondPhaseObjectListCounterBuffer = builder.Write(clear_counter_pass->secondPhaseObjectListCounterBuffer);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.hzbTexture = builder.Read(pHZB->Get1stPhaseCullingHZBMip(i), i);
            }
        },
        [=](const InstanceCullingData& data, IGfxCommandList* pCommandList)
        {
            InstanceCulling1stPhase(pCommandList, 
                pRenderGraph->GetBuffer(data.cullingResultBuffer),
                pRenderGraph->GetBuffer(data.secondPhaseObjectListBuffer),
                pRenderGraph->GetBuffer(data.secondPhaseObjectListCounterBuffer));
        });

    struct BuildMeshletListData
    {
        RGHandle cullingResultBuffer;
        RGHandle meshletListBuffer;
        RGHandle meshletListCounterBuffer;
    };

    auto build_meshlet_list_pass = pRenderGraph->AddPass<BuildMeshletListData>("Build Meshlet List", RenderPassType::Compute,
        [&](BuildMeshletListData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint2);
            bufferDesc.size = bufferDesc.stride * max_meshlets_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer;
            data.meshletListBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase meshlet list");

            data.cullingResultBuffer = builder.Read(instance_culling_pass->cullingResultBuffer);
            data.meshletListBuffer = builder.Write(data.meshletListBuffer);
            data.meshletListCounterBuffer = builder.Write(clear_counter_pass->firstPhaseMeshletListCounterBuffer);
        },
        [=](const BuildMeshletListData& data, IGfxCommandList* pCommandList)
        {
            BuildMeshletList(pCommandList, 
                pRenderGraph->GetBuffer(data.cullingResultBuffer),
                pRenderGraph->GetBuffer(data.meshletListBuffer),
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer));
        });

    struct BuildIndirectCommandPassData
    {
        RGHandle meshletListCounterBuffer;
        RGHandle indirectCommandBuffer;
    };

    auto build_indirect_command = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command", RenderPassType::Compute,
        [&](BuildIndirectCommandPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint3);
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer;
            data.indirectCommandBuffer = builder.Create<RGBuffer>(bufferDesc, "1st Phase Indirect Command");
            data.indirectCommandBuffer = builder.Write(data.indirectCommandBuffer);

            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer);
        },
        [=](const BuildIndirectCommandPassData& data, IGfxCommandList* pCommandList)
        {
            BuildIndirectCommand(pCommandList, 
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer),
                pRenderGraph->GetBuffer(data.indirectCommandBuffer));
        });

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass", RenderPassType::Graphics,
        [&](BasePassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.width = m_pRenderer->GetRenderWidth();
            desc.height = m_pRenderer->GetRenderHeight();
            desc.format = GfxFormat::RGBA8SRGB;
            data.outDiffuseRT = builder.Create<RGTexture>(desc, "Diffuse RT");
            data.outSpecularRT = builder.Create<RGTexture>(desc, "Specular RT");

            desc.format = GfxFormat::RGBA8UNORM;
            data.outNormalRT = builder.Create<RGTexture>(desc, "Normal RT");
            data.outCustomRT = builder.Create<RGTexture>(desc, "CustomData RT");

            desc.format = GfxFormat::R11G11B10F;
            data.outEmissiveRT = builder.Create<RGTexture>(desc, "Emissive RT");

            desc.format = GfxFormat::D32F;
            data.outDepthRT = builder.Create<RGTexture>(desc, "SceneDepth RT");

            data.outDiffuseRT = builder.WriteColor(0, data.outDiffuseRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outSpecularRT = builder.WriteColor(1, data.outSpecularRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outNormalRT = builder.WriteColor(2, data.outNormalRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outEmissiveRT = builder.WriteColor(3, data.outEmissiveRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outCustomRT = builder.WriteColor(4, data.outCustomRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outDepthRT = builder.WriteDepth(data.outDepthRT, 0, GfxRenderPassLoadOp::Clear, GfxRenderPassLoadOp::Clear);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.inHZB = builder.Read(pHZB->Get1stPhaseCullingHZBMip(i), i, RGBuilderFlag::ShaderStageNonPS);
            }

            data.indirectCommandBuffer = builder.ReadIndirectArg(build_indirect_command->indirectCommandBuffer);
            data.meshletListBuffer = builder.Read(build_meshlet_list_pass->meshletListBuffer, 0, RGBuilderFlag::ShaderStageNonPS);
            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, 0, RGBuilderFlag::ShaderStageNonPS);

            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint2);
            bufferDesc.size = bufferDesc.stride * max_meshlets_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer;
            data.occlusionCulledMeshletsBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase meshlet list");
            data.occlusionCulledMeshletsBuffer = builder.Write(data.occlusionCulledMeshletsBuffer);

            data.occlusionCulledMeshletsCounterBuffer = builder.Write(clear_counter_pass->secondPhaseMeshletListCounterBuffer);
        },
        [=](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            Flush1stPhaseBatches(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectCommandBuffer),
                pRenderGraph->GetBuffer(data.meshletListBuffer),
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer));
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_customDataRT = gbuffer_pass->outCustomRT;
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
        RGHandle objectListCounterBuffer;
        RGHandle commandBuffer;
    };
    auto build_culling_command = pRenderGraph->AddPass<BuildCullingCommandData>("Build Instance Culling Command", RenderPassType::Compute,
        [&](BuildCullingCommandData& data, RGBuilder& builder)
        {
            RGBuffer::Desc desc;
            desc.stride = sizeof(uint3);
            desc.size = desc.stride;
            desc.usage = GfxBufferUsageStructuredBuffer;
            data.commandBuffer = builder.Create<RGBuffer>(desc, "2nd phase instance culling command");
            data.commandBuffer = builder.Write(data.commandBuffer);

            data.objectListCounterBuffer = builder.Read(m_secondPhaseObjectListCounterBuffer);
        },
        [=](const BuildCullingCommandData& data, IGfxCommandList* pCommandList)
        {
            RGBuffer* commandBuffer = pRenderGraph->GetBuffer(data.commandBuffer);
            RGBuffer* objectListCounterBuffer = pRenderGraph->GetBuffer(data.objectListCounterBuffer);

            pCommandList->SetPipelineState(m_pBuildInstanceCullingCommandPSO);

            uint32_t consts[2] = { commandBuffer->GetUAV()->GetHeapIndex(), objectListCounterBuffer->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, consts, sizeof(consts));
            pCommandList->Dispatch(1, 1, 1);
        });

    struct InstanceCullingData
    {
        RGHandle indirectCommandBuffer;
        RGHandle hzbTexture;
        RGHandle cullingResultBuffer; //[true, false, false, ...] for each instance
        RGHandle objectListBuffer;
        RGHandle objectListCounterBuffer;
    };

    auto instance_culling_pass = pRenderGraph->AddPass<InstanceCullingData>("Instance Culling", RenderPassType::Compute,
        [&](InstanceCullingData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = 1;
            bufferDesc.size = bufferDesc.stride * max_instance_num;
            bufferDesc.format = GfxFormat::R8UI;
            bufferDesc.usage = GfxBufferUsageTypedBuffer;
            data.cullingResultBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase culling result");
            data.cullingResultBuffer = builder.Write(data.cullingResultBuffer);

            data.indirectCommandBuffer = builder.ReadIndirectArg(build_culling_command->commandBuffer);
            data.objectListBuffer = builder.Read(m_secondPhaseObjectListBuffer);
            data.objectListCounterBuffer = builder.Read(m_secondPhaseObjectListCounterBuffer);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.hzbTexture = builder.Read(pHZB->Get2ndPhaseCullingHZBMip(i), i);
            }
        },
        [=](const InstanceCullingData& data, IGfxCommandList* pCommandList)
        {
            InstanceCulling2ndPhase(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectCommandBuffer), 
                pRenderGraph->GetBuffer(data.cullingResultBuffer), 
                pRenderGraph->GetBuffer(data.objectListBuffer),
                pRenderGraph->GetBuffer(data.objectListCounterBuffer));
        });

    struct BuildMeshletListData
    {
        RGHandle cullingResultBuffer;
        RGHandle meshletListBuffer;
        RGHandle meshletListCounterBuffer;
    };

    auto build_meshlet_list_pass = pRenderGraph->AddPass<BuildMeshletListData>("Build Meshlet List", RenderPassType::Compute,
        [&](BuildMeshletListData& data, RGBuilder& builder)
        {
            data.cullingResultBuffer = builder.Read(instance_culling_pass->cullingResultBuffer);
            data.meshletListBuffer = builder.Write(m_secondPhaseMeshletListBuffer);
            data.meshletListCounterBuffer = builder.Write(m_secondPhaseMeshletListCounterBuffer);
        },
        [=](const BuildMeshletListData& data, IGfxCommandList* pCommandList)
        {
            BuildMeshletList(pCommandList, 
                pRenderGraph->GetBuffer(data.cullingResultBuffer),
                pRenderGraph->GetBuffer(data.meshletListBuffer),
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer));
        });

    struct BuildIndirectCommandPassData
    {
        RGHandle meshletListCounterBuffer;
        RGHandle indirectCommandBuffer;
    };

    auto build_indirect_command = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command", RenderPassType::Compute,
        [&](BuildIndirectCommandPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint3);
            bufferDesc.size = bufferDesc.stride * max_dispatch_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer;
            data.indirectCommandBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd Phase Indirect Command");
            data.indirectCommandBuffer = builder.Write(data.indirectCommandBuffer);

            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer);
        },
        [=](const BuildIndirectCommandPassData& data, IGfxCommandList* pCommandList)
        {
            BuildIndirectCommand(pCommandList, 
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer),
                pRenderGraph->GetBuffer(data.indirectCommandBuffer));
        });

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass", RenderPassType::Graphics,
        [&](BasePassData& data, RGBuilder& builder)
        {
            data.outDiffuseRT = builder.WriteColor(0, m_diffuseRT, 0, GfxRenderPassLoadOp::Load);
            data.outSpecularRT = builder.WriteColor(1, m_specularRT, 0, GfxRenderPassLoadOp::Load);
            data.outNormalRT = builder.WriteColor(2, m_normalRT, 0, GfxRenderPassLoadOp::Load);
            data.outEmissiveRT = builder.WriteColor(3, m_emissiveRT, 0, GfxRenderPassLoadOp::Load);
            data.outCustomRT = builder.WriteColor(4, m_customDataRT, 0, GfxRenderPassLoadOp::Load);
            data.outDepthRT = builder.WriteDepth(m_depthRT, 0, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load);

            for (uint32_t i = 0; i < pHZB->GetHZBMipCount(); ++i)
            {
                data.inHZB = builder.Read(pHZB->Get2ndPhaseCullingHZBMip(i), i, RGBuilderFlag::ShaderStageNonPS);
            }

            data.meshletListBuffer = builder.Read(build_meshlet_list_pass->meshletListBuffer, 0, RGBuilderFlag::ShaderStageNonPS);
            data.meshletListCounterBuffer = builder.Read(build_meshlet_list_pass->meshletListCounterBuffer, 0, RGBuilderFlag::ShaderStageNonPS);
            data.indirectCommandBuffer = builder.ReadIndirectArg(build_indirect_command->indirectCommandBuffer);
        },
        [=](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            Flush2ndPhaseBatches(pCommandList, 
                pRenderGraph->GetBuffer(data.indirectCommandBuffer), 
                pRenderGraph->GetBuffer(data.meshletListBuffer), 
                pRenderGraph->GetBuffer(data.meshletListCounterBuffer));
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_customDataRT = gbuffer_pass->outCustomRT;
    m_depthRT = gbuffer_pass->outDepthRT;
}

void BasePass::MergeBatches()
{
    m_nTotalInstanceCount = (uint32_t)m_instances.size();

    eastl::vector<uint32_t> instanceIndices(m_nTotalInstanceCount);
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
        eastl::vector<RenderBatch> batches;
        uint32_t meshletCount;
    };
    eastl::map<IGfxPipelineState*, MergedBatch> mergedBatches;

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
                mergedBatches.insert(eastl::make_pair(batch.pso, mergedBatch));
            }
        }
        else
        {
            m_nonGpuDrivenBatches.push_back(batch); //todo : we can also do instance-culling and merging for VS batches
        }
    }

    uint32_t meshletListOffset = 0;
    for (auto iter = mergedBatches.begin(); iter != mergedBatches.end(); ++iter)
    {
        const MergedBatch& batch = iter->second;

        eastl::vector<uint2> meshletList;
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

void BasePass::ResetCounter(IGfxCommandList* pCommandList, RGBuffer* firstPhaseMeshletCounter, RGBuffer* secondPhaseObjectCounter, RGBuffer* secondPhaseMeshletCounter)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(firstPhaseMeshletCounter->GetBuffer(), firstPhaseMeshletCounter->GetUAV(), clear_value);
    pCommandList->ClearUAV(secondPhaseObjectCounter->GetBuffer(), secondPhaseObjectCounter->GetUAV(), clear_value);
    pCommandList->ClearUAV(secondPhaseMeshletCounter->GetBuffer(), secondPhaseMeshletCounter->GetUAV(), clear_value);

    pCommandList->UavBarrier(firstPhaseMeshletCounter->GetBuffer());
    pCommandList->UavBarrier(secondPhaseObjectCounter->GetBuffer());
    pCommandList->UavBarrier(secondPhaseMeshletCounter->GetBuffer());
}

void BasePass::InstanceCulling1stPhase(IGfxCommandList* pCommandList, RGBuffer* cullingResultUAV, RGBuffer* secondPhaseObjectListUAV, RGBuffer* secondPhaseObjectListCounterUAV)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(cullingResultUAV->GetBuffer(), cullingResultUAV->GetUAV(), clear_value);
    pCommandList->UavBarrier(cullingResultUAV->GetBuffer());

    pCommandList->SetPipelineState(m_p1stPhaseInstanceCullingPSO);

    uint32_t instance_count = m_nTotalInstanceCount;
    uint32_t root_consts[5] = { 
        m_instanceIndexAddress, 
        instance_count, 
        cullingResultUAV->GetUAV()->GetHeapIndex(),
        secondPhaseObjectListUAV->GetUAV()->GetHeapIndex(),
        secondPhaseObjectListCounterUAV->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    uint32_t group_count = max((instance_count + 63) / 64, 1u); //avoid empty dispatch warning
    pCommandList->Dispatch(group_count, 1, 1);
}

void BasePass::InstanceCulling2ndPhase(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* cullingResultUAV, RGBuffer* objectListBufferSRV, RGBuffer* objectListCounterBufferSRV)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(cullingResultUAV->GetBuffer(), cullingResultUAV->GetUAV(), clear_value);
    pCommandList->UavBarrier(cullingResultUAV->GetBuffer());

    pCommandList->SetPipelineState(m_p2ndPhaseInstanceCullingPSO);

    uint32_t root_consts[3] = { objectListBufferSRV->GetSRV()->GetHeapIndex(), objectListCounterBufferSRV->GetSRV()->GetHeapIndex(), cullingResultUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->DispatchIndirect(pIndirectCommandBuffer->GetBuffer(), 0);
}

void BasePass::Flush1stPhaseBatches(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.pso);

        uint32_t root_consts[5] = { pMeshletListSRV->GetSRV()->GetHeapIndex(), pMeshletListCounterSRV->GetSRV()->GetHeapIndex(), batch.meshletListBufferOffset, (uint32_t)i, 1};
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer->GetBuffer(), sizeof(uint3) * (uint32_t)i);
    }
}

void BasePass::Flush2ndPhaseBatches(IGfxCommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.pso);

        uint32_t root_consts[5] = { pMeshletListSRV->GetSRV()->GetHeapIndex(), pMeshletListCounterSRV->GetSRV()->GetHeapIndex(), batch.meshletListBufferOffset, (uint32_t)i, 0};
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer->GetBuffer(), sizeof(uint3) * (uint32_t)i);
    }

    for (size_t i = 0; i < m_nonGpuDrivenBatches.size(); ++i)
    {
        DrawBatch(pCommandList, m_nonGpuDrivenBatches[i]);
    }
}

void BasePass::BuildMeshletList(IGfxCommandList* pCommandList, RGBuffer* cullingResultSRV, RGBuffer* meshletListBufferUAV, RGBuffer* meshletListCounterBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildMeshletListPSO);

    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        uint32_t consts[7] = {
            (uint32_t)i,
            cullingResultSRV->GetSRV()->GetHeapIndex(),
            m_indirectBatches[i].originMeshletListAddress,
            m_indirectBatches[i].originMeshletCount,
            m_indirectBatches[i].meshletListBufferOffset,
            meshletListBufferUAV->GetUAV()->GetHeapIndex(),
            meshletListCounterBufferUAV->GetUAV()->GetHeapIndex()};
        pCommandList->SetComputeConstants(1, consts, sizeof(consts));
        pCommandList->Dispatch(DivideRoudingUp(m_indirectBatches[i].originMeshletCount, 64), 1, 1);
    }
}

void BasePass::BuildIndirectCommand(IGfxCommandList* pCommandList, RGBuffer* pCounterBufferSRV, RGBuffer* pCommandBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildIndirectCommandPSO);

    uint32_t batch_count = (uint32_t)m_indirectBatches.size();

    uint32_t consts[3] = { batch_count, pCounterBufferSRV->GetSRV()->GetHeapIndex(), pCommandBufferUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, consts, sizeof(consts));

    uint32_t group_count = max((batch_count + 63) / 64, 1u); //avoid empty dispatch warning
    pCommandList->Dispatch(group_count, 1, 1);
}
