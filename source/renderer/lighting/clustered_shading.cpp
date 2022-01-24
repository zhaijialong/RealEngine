#include "clustered_shading.h"
#include "../renderer.h"
#include "core/engine.h"

ClusteredShading::ClusteredShading(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("clustered_shading.hlsl", "main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "ClusteredShading PSO");
}

RenderGraphHandle ClusteredShading::Render(RenderGraph* pRenderGraph, RenderGraphHandle diffuseRT, RenderGraphHandle specularRT, RenderGraphHandle normalRT, RenderGraphHandle emissiveRT,
    RenderGraphHandle depthRT, RenderGraphHandle shadowRT, RenderGraphHandle ao, uint32_t width, uint32_t height)
{
    auto cluster_shading_pass = pRenderGraph->AddPass<ClusterShadingPassData>("ClusterShading",
        [&](ClusterShadingPassData& data, RenderGraphBuilder& builder)
        {
            data.inDiffuseRT = builder.Read(diffuseRT, GfxResourceState::ShaderResourceNonPS);
            data.inSpecularRT = builder.Read(specularRT, GfxResourceState::ShaderResourceNonPS);
            data.inNormalRT = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);
            data.inEmissiveRT = builder.Read(emissiveRT, GfxResourceState::ShaderResourceNonPS);
            data.inDepthRT = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.inShadowRT = builder.Read(shadowRT, GfxResourceState::ShaderResourceNonPS);
            data.inAOTermRT = builder.Read(ao, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageUnorderedAccess;
            data.outHdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor RT");

            data.outHdrRT = builder.Write(data.outHdrRT, GfxResourceState::UnorderedAccess);
        },
        [=](const ClusterShadingPassData& data, IGfxCommandList* pCommandList)
        {
            Draw(pCommandList, data, width, height);
        });

    return cluster_shading_pass->outHdrRT;
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList, const ClusterShadingPassData& data, uint32_t width, uint32_t height)
{
    RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();

    RenderGraphTexture* diffuseRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inDiffuseRT);
    RenderGraphTexture* specularRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inSpecularRT);
    RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inNormalRT);
    RenderGraphTexture* emissiveRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inEmissiveRT);
    RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inDepthRT);
    RenderGraphTexture* shadowRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inShadowRT);
    RenderGraphTexture* gtaoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inAOTermRT);
    RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outHdrRT);

    pCommandList->SetPipelineState(m_pPSO);

    struct CB1
    {
        uint diffuseRT;
        uint specularRT;
        uint normalRT;
        uint emissiveRT;

        uint depthRT;
        uint shadowRT;
        uint gtaoRT;
        uint hdrRT;
    };

    CB1 cb1;
    cb1.diffuseRT = diffuseRT->GetSRV()->GetHeapIndex();
    cb1.specularRT = specularRT->GetSRV()->GetHeapIndex();
    cb1.normalRT = normalRT->GetSRV()->GetHeapIndex();
    cb1.emissiveRT = emissiveRT->GetSRV()->GetHeapIndex();
    cb1.depthRT = depthRT->GetSRV()->GetHeapIndex();
    cb1.shadowRT = shadowRT->GetSRV()->GetHeapIndex();
    cb1.gtaoRT = gtaoRT->GetSRV()->GetHeapIndex();
    cb1.hdrRT = hdrRT->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb1, sizeof(cb1));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
