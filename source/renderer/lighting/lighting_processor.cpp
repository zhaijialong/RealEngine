#include "lighting_processor.h"

LightingProcessor::LightingProcessor(Renderer* pRenderer)
{
    m_pGTAO.reset(new GTAO(pRenderer));
    m_pClusteredShading.reset(new ClusteredShading(pRenderer));
}

RenderGraphHandle LightingProcessor::Process(RenderGraph* pRenderGraph, const LightingProcessInput& input, uint32_t width, uint32_t height)
{
    auto gtao_filter_depth_pass = pRenderGraph->AddPass<GTAOFilterDepthPassData>("GTAO filter depth",
        [&](GTAOFilterDepthPassData& data, RenderGraphBuilder& builder)
        {
            data.inputDepth = builder.Read(input.depthRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.mip_levels = 5;
            desc.format = GfxFormat::R16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            RenderGraphHandle gtao_depth = builder.Create<RenderGraphTexture>(desc, "GTAO depth(HZB)");

            data.outputDepthMip0 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 0);
            data.outputDepthMip1 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 1);
            data.outputDepthMip2 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 2);
            data.outputDepthMip3 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 3);
            data.outputDepthMip4 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 4);
        },
        [=](const GTAOFilterDepthPassData& data, IGfxCommandList* pCommandList)
        {
            m_pGTAO->FilterDepth(pCommandList, data, width, height);
        });

    auto gtao_pass = pRenderGraph->AddPass<GTAOPassData>("GTAO",
        [&](GTAOPassData& data, RenderGraphBuilder& builder)
        {
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip0, GfxResourceState::ShaderResourceNonPS, 0);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip1, GfxResourceState::ShaderResourceNonPS, 1);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip2, GfxResourceState::ShaderResourceNonPS, 2);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip3, GfxResourceState::ShaderResourceNonPS, 3);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip4, GfxResourceState::ShaderResourceNonPS, 4);

            data.inputNormal = builder.Read(input.normalRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputAOTerm = builder.Create<RenderGraphTexture>(desc, "GTAO AOTerm");
            data.outputAOTerm = builder.Write(data.outputAOTerm, GfxResourceState::UnorderedAccess);

            desc.format = GfxFormat::R8UNORM;
            data.outputEdge = builder.Create<RenderGraphTexture>(desc, "GTAO Edge");
            data.outputEdge = builder.Write(data.outputEdge, GfxResourceState::UnorderedAccess);
        },
        [=](const GTAOPassData& data, IGfxCommandList* pCommandList)
        {
            m_pGTAO->Draw(pCommandList, data, width, height);
        });

    auto gtao_denoise_pass = pRenderGraph->AddPass<GTAODenoisePassData>("GTAO denoise",
        [&](GTAODenoisePassData& data, RenderGraphBuilder& builder)
        {
            data.inputAOTerm = builder.Read(gtao_pass->outputAOTerm, GfxResourceState::ShaderResourceNonPS);
            data.inputEdge = builder.Read(gtao_pass->outputEdge, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R32UI;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputAOTerm = builder.Create<RenderGraphTexture>(desc, "GTAO denoised AO");
            data.outputAOTerm = builder.Write(data.outputAOTerm, GfxResourceState::UnorderedAccess);
        },
        [=](const GTAODenoisePassData& data, IGfxCommandList* pCommandList)
        {
            m_pGTAO->Denoise(pCommandList, data, width, height);
        });

    auto cluster_shading_pass = pRenderGraph->AddPass<ClusterShadingPassData>("ClusterShading",
        [&](ClusterShadingPassData& data, RenderGraphBuilder& builder)
        {
            data.inAlbedoRT = builder.Read(input.albedoRT, GfxResourceState::ShaderResourceNonPS);
            data.inNormalRT = builder.Read(input.normalRT, GfxResourceState::ShaderResourceNonPS);
            data.inEmissiveRT = builder.Read(input.emissiveRT, GfxResourceState::ShaderResourceNonPS);
            data.inDepthRT = builder.Read(input.depthRT, GfxResourceState::ShaderResourceNonPS);
            data.inShadowRT = builder.Read(input.shadowRT, GfxResourceState::ShaderResourceNonPS);
            data.inAOTermRT = builder.Read(gtao_denoise_pass->outputAOTerm, GfxResourceState::ShaderResourceNonPS);

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
        });

    return cluster_shading_pass->outHdrRT;
}
