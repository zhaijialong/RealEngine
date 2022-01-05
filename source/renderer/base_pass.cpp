#include "base_pass.h"
#include "renderer.h"
#include "hierarchical_depth_buffer.h"

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
    return m_batchs.emplace_back(*allocator);
}

void BasePass::Render(RenderGraph* pRenderGraph)
{
    HZB* pHZB = m_pRenderer->GetHZB();

    auto gbuffer_pass = pRenderGraph->AddPass<BasePassData>("GBuffer Pass",
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
            for (size_t i = 0; i < m_batchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_batchs[i]);
            }
            m_batchs.clear();
        });

    m_diffuseRT = gbuffer_pass->outDiffuseRT;
    m_specularRT = gbuffer_pass->outSpecularRT;
    m_normalRT = gbuffer_pass->outNormalRT;
    m_emissiveRT = gbuffer_pass->outEmissiveRT;
    m_depthRT = gbuffer_pass->outDepthRT;
}