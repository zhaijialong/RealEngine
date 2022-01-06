#include "base_pass.h"
#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "utils/profiler.h"

struct BasePassData
{
    RenderGraphHandle inHZB;

    RenderGraphHandle outDiffuseRT;  //srgb : diffuse(xyz) + ao(a)
    RenderGraphHandle outSpecularRT; //srgb : specular(xyz), a: not used
    RenderGraphHandle outNormalRT;   //rgba8norm : normal(xyz) + roughness(a)
    RenderGraphHandle outEmissiveRT; //r11g11b10 : emissive
    RenderGraphHandle outDepthRT;
};

BasePass::BasePass(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderBatch& BasePass::AddBatch()
{
    LinearAllocator* allocator = m_pRenderer->GetConstantAllocator();
    return m_batches.emplace_back(*allocator);
}

void BasePass::Render1stPhase(RenderGraph* pRenderGraph)
{
    MergeBatches();

    HZB* pHZB = m_pRenderer->GetHZB();

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
}

void BasePass::Render2ndPhase(RenderGraph* pRenderGraph)
{
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
        },
        [&](const BasePassData& data, IGfxCommandList* pCommandList)
        {
            Flush2ndPhaseBatches(pCommandList);
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;
}

void BasePass::MergeBatches()
{
    for (size_t i = 0; i < m_batches.size(); ++i)
    {
        const RenderBatch& batch = m_batches[i];
        if (batch.pso->GetType() == GfxPipelineType::MeshShading)
        {
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

        uint32_t root_consts[2] = {dataPerMeshletAddress, batch.meshletCount};
        pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

        pCommandList->DispatchMesh((batch.meshletCount + 31) / 32, 1, 1); //hard coded 32 group size for AS
    }

    m_mergedBatches.clear();
}

void BasePass::Flush2ndPhaseBatches(IGfxCommandList* pCommandList)
{
    for (size_t i = 0; i < m_nonMergedBatches.size(); ++i)
    {
        DrawBatch(pCommandList, m_nonMergedBatches[i]);
    }
    m_nonMergedBatches.clear();
}