#include "lighting_processor.h"

LightingProcessor::LightingProcessor(Renderer* pRenderer)
{
    m_pClusteredShading.reset(new ClusteredShading(pRenderer));
}

RenderGraphHandle LightingProcessor::Process(RenderGraph* pRenderGraph, const LightingProcessInput& input, uint32_t width, uint32_t height)
{
    auto cluster_shading_pass = pRenderGraph->AddPass<ClusterShadingPassData>("ClusterShading",
        [&](ClusterShadingPassData& data, RenderGraphBuilder& builder)
        {
            data.inAlbedoRT = builder.Read(input.albedoRT, GfxResourceState::ShaderResourceNonPS);
            data.inNormalRT = builder.Read(input.normalRT, GfxResourceState::ShaderResourceNonPS);
            data.inEmissiveRT = builder.Read(input.emissiveRT, GfxResourceState::ShaderResourceNonPS);
            data.inDepthRT = builder.Read(input.depthRT, GfxResourceState::ShaderResourceNonPS);
            data.inShadowRT = builder.Read(input.shadowRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outHdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor RT");

            data.outHdrRT = builder.Write(data.outHdrRT, GfxResourceState::UnorderedAccess);
        },
        [=](const ClusterShadingPassData& data, IGfxCommandList* pCommandList)
        {
            m_pClusteredShading->Draw(pCommandList, data, width, height);
        }
        );

    return cluster_shading_pass->outHdrRT;
}
