#include "base_pass.h"
#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "utils/profiler.h"

struct BasePassData
{
    RenderGraphHandle inHZB;
    RenderGraphHandle occlusionCulledMeshletsBuffer;
    RenderGraphHandle occlusionCulledMeshletsCounterBuffer;
    RenderGraphHandle indirectCommandBuffer;

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
    desc.cs = pRenderer->GetShader("instance_culling.hlsl", "build_2nd_phase_indirect_command", "cs_6_6", {});
    m_pBuildIndirectCommandPSO = pRenderer->GetPipelineState(desc, "2nd phase indirect command PSO");
}

RenderBatch& BasePass::AddBatch()
{
    LinearAllocator* allocator = m_pRenderer->GetConstantAllocator();
    return m_batches.emplace_back(*allocator);
}

void BasePass::Render1stPhase(RenderGraph* pRenderGraph)
{
    MergeBatches();

    uint32_t max_counter_num = roundup((uint32_t)m_mergedBatches.size(), 65536 / sizeof(uint));
    uint32_t max_meshlets_num = roundup(m_nTotalMeshletCount, 65536 / sizeof(uint2));

    HZB* pHZB = m_pRenderer->GetHZB();

    struct ClearCounterPassData
    {
        RenderGraphHandle counterBuffer;
    };
    auto clear_counter_pass = pRenderGraph->AddPass<ClearCounterPassData>("Clear Counter",
        [&](ClearCounterPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = 4;
            bufferDesc.size = bufferDesc.stride * max_counter_num;
            bufferDesc.format = GfxFormat::R32F;
            bufferDesc.usage = GfxBufferUsageRawBuffer | GfxBufferUsageUnorderedAccess;
            data.counterBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "Occlusion Culled Meshlets Counter");
            data.counterBuffer = builder.Write(data.counterBuffer, GfxResourceState::UnorderedAccess);
        },
        [=](const ClearCounterPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* buffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.counterBuffer);

            uint32_t clear_value[4] = { 0, 0, 0, 0 };
            pCommandList->ClearUAV(buffer->GetBuffer(), buffer->GetUAV(), clear_value);
            pCommandList->UavBarrier(buffer->GetBuffer()); //todo : support uav barrier in rendergraph
        });

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass 1st phase",
        [&](BasePassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_pRenderer->GetBackbufferWidth();
            desc.height = m_pRenderer->GetBackbufferHeight();
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            desc.format = GfxFormat::RGBA8SRGB;
            data.outDiffuseRT = builder.Create<RenderGraphTexture>(desc, "Diffuse RT");
            data.outSpecularRT = builder.Create<RenderGraphTexture>(desc, "Specular RT");

            desc.format = GfxFormat::RGBA8UNORM;
            data.outNormalRT = builder.Create<RenderGraphTexture>(desc, "Normal RT");

            desc.format = GfxFormat::R11G11B10F;
            data.outEmissiveRT = builder.Create<RenderGraphTexture>(desc, "Emissive RT");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
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

            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint2);
            bufferDesc.size = bufferDesc.stride * max_meshlets_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.occlusionCulledMeshletsBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "Occlusion Culled Meshlets");
            data.occlusionCulledMeshletsBuffer = builder.Write(data.occlusionCulledMeshletsBuffer, GfxResourceState::UnorderedAccess);

            data.occlusionCulledMeshletsCounterBuffer = builder.Write(clear_counter_pass->counterBuffer, GfxResourceState::UnorderedAccess);
        },
        [&](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            Flush1stPhaseBatches(pCommandList);
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;

    m_occlusionCulledMeshletsBuffer = gbuffer_pass->occlusionCulledMeshletsBuffer;
    m_occlusionCulledMeshletsCounterBuffer = gbuffer_pass->occlusionCulledMeshletsCounterBuffer;
}

void BasePass::Render2ndPhase(RenderGraph* pRenderGraph)
{
    struct BuildIndirectCommandPassData
    {
        RenderGraphHandle culledMeshletsCounterBuffer;
        RenderGraphHandle indirectCommandBuffer;
    };

    uint32_t max_counter_num = roundup((uint32_t)m_mergedBatches.size(), 65536 / sizeof(uint));

    auto build_indirect_command = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command",
        [&](BuildIndirectCommandPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint3);
            bufferDesc.size = bufferDesc.stride * max_counter_num;
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.indirectCommandBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "2nd Phase Indirect Command");
            data.indirectCommandBuffer = builder.Write(data.indirectCommandBuffer, GfxResourceState::UnorderedAccess);

            data.culledMeshletsCounterBuffer = builder.Read(m_occlusionCulledMeshletsCounterBuffer, GfxResourceState::ShaderResourceNonPS);
        },
        [=](const BuildIndirectCommandPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* counterBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.culledMeshletsCounterBuffer);
            RenderGraphBuffer* commandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            BuildIndirectCommand(pCommandList, counterBuffer->GetSRV(), commandBuffer->GetUAV());
        });

    HZB* pHZB = m_pRenderer->GetHZB();

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("Base Pass 2nd phase",
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

            data.occlusionCulledMeshletsBuffer = builder.Read(m_occlusionCulledMeshletsBuffer, GfxResourceState::ShaderResourceNonPS);
            data.occlusionCulledMeshletsCounterBuffer = builder.Read(m_occlusionCulledMeshletsCounterBuffer, GfxResourceState::ShaderResourceNonPS);
            data.indirectCommandBuffer = builder.Read(build_indirect_command->indirectCommandBuffer, GfxResourceState::IndirectArg);
        },
        [=](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* indirectCommandBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectCommandBuffer);
            Flush2ndPhaseBatches(pCommandList, indirectCommandBuffer->GetBuffer());
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;
}

void BasePass::MergeBatches()
{
    m_nTotalMeshletCount = 0;

    for (size_t i = 0; i < m_batches.size(); ++i)
    {
        const RenderBatch& batch = m_batches[i];
        if (batch.pso->GetType() == GfxPipelineType::MeshShading)
        {
            m_nTotalMeshletCount += batch.meshletCount;

            auto iter = m_mergedBatches.find(batch.pso);
            if (iter != m_mergedBatches.end())
            {
                iter->second.meshletCount += batch.meshletCount;
                iter->second.batches.push_back(batch);
            }
            else
            {
                MergedBatch mergedBatch;
                mergedBatch.batches.push_back(batch);
                mergedBatch.meshletCount = batch.meshletCount;
                m_mergedBatches.insert(std::make_pair(batch.pso, mergedBatch));
            }
        }
        else
        {
            m_nonMergedBatches.push_back(batch);
        }
    }

    m_batches.clear();
}

void BasePass::Flush1stPhaseBatches(IGfxCommandList* pCommandList)
{
    CPU_EVENT("Render", "BasePass");

    uint32_t occlusionCulledMeshletsBufferOffset = 0;
    uint32_t dispatchIndex = 0;

    for (auto iter = m_mergedBatches.begin(); iter != m_mergedBatches.end(); ++iter)
    {
        IGfxPipelineState* pso = iter->first;
        const MergedBatch& batch = iter->second;

        std::string event_name = "MergedBatch(" + std::to_string(batch.batches.size()) + " batches, " + std::to_string(batch.meshletCount) + " meshlets)";
        GPU_EVENT_DEBUG(pCommandList, event_name);

        pCommandList->SetPipelineState(pso);

        std::vector<uint2> dataPerMeshlet;
        dataPerMeshlet.reserve(batch.meshletCount);

        for (size_t i = 0; i < batch.batches.size(); ++i)
        {
            uint32_t sceneConstantAddress = batch.batches[i].sceneConstantAddress;
            for (size_t m = 0; m < batch.batches[i].meshletCount; ++m)
            {
                dataPerMeshlet.emplace_back(sceneConstantAddress, (uint32_t)m);
            }
        }

        uint32_t dataPerMeshletAddress = m_pRenderer->AllocateSceneConstant(dataPerMeshlet.data(), sizeof(uint2) * (uint32_t)dataPerMeshlet.size());

        uint32_t root_consts[5] = { dataPerMeshletAddress, occlusionCulledMeshletsBufferOffset, dispatchIndex, 1, batch.meshletCount };
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMesh((batch.meshletCount + 31) / 32, 1, 1); //hard coded 32 group size for AS

        //second phase batch
        m_indirectBatches.push_back({ pso, occlusionCulledMeshletsBufferOffset });

        occlusionCulledMeshletsBufferOffset += batch.meshletCount;
        dispatchIndex++;
    }

    m_mergedBatches.clear();
}

void BasePass::Flush2ndPhaseBatches(IGfxCommandList* pCommandList, IGfxBuffer* pIndirectCommandBuffer)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.pso);

        uint32_t root_consts[5] = { 0, batch.occlusionCulledMeshletsBufferOffset, (uint32_t)i, 0, 0 };
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer, sizeof(uint3) * (uint32_t)i);
    }
    m_indirectBatches.clear();

    for (size_t i = 0; i < m_nonMergedBatches.size(); ++i)
    {
        DrawBatch(pCommandList, m_nonMergedBatches[i]);
    }
    m_nonMergedBatches.clear();
}

void BasePass::BuildIndirectCommand(IGfxCommandList* pCommandList, IGfxDescriptor* pCounterBufferSRV, IGfxDescriptor* pCommandBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildIndirectCommandPSO);

    uint32_t batch_count = (uint32_t)m_indirectBatches.size();

    uint32_t consts[3] = { batch_count, pCounterBufferSRV->GetHeapIndex(), pCommandBufferUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, consts, sizeof(consts));

    pCommandList->Dispatch((batch_count + 63) / 64, 1, 1);
}
