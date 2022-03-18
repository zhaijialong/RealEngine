#include "lighting_processor.h"
#include "../renderer.h"
#include "../base_pass.h"

LightingProcessor::LightingProcessor(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    m_pGTAO = eastl::make_unique<GTAO>(pRenderer);
    m_pRTShdow = eastl::make_unique<RTShadow>(pRenderer);
    m_pClusteredShading = eastl::make_unique<ClusteredShading>(pRenderer);
    m_pReflection = eastl::make_unique<HybridStochasticReflection>(pRenderer);
}

RenderGraphHandle LightingProcessor::Render(RenderGraph* pRenderGraph, RenderGraphHandle depth, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Lighting");

    BasePass* pBasePass = m_pRenderer->GetBassPass();
    RenderGraphHandle diffuse = pBasePass->GetDiffuseRT();
    RenderGraphHandle specular = pBasePass->GetSpecularRT();
    RenderGraphHandle normal = pBasePass->GetNormalRT();
    RenderGraphHandle emissive = pBasePass->GetEmissiveRT();

    RenderGraphHandle gtao = m_pGTAO->Render(pRenderGraph, depth, normal, width, height);
    RenderGraphHandle shadow = m_pRTShdow->Render(pRenderGraph, depth, normal, width, height);
    RenderGraphHandle direct_lighting = m_pClusteredShading->Render(pRenderGraph, diffuse, specular, normal, depth, shadow, width, height);
    RenderGraphHandle indirect_specular = m_pReflection->Render(pRenderGraph, width, height);
    //RenderGraphHandle indirect_diffuse = todo : diffuse GI

    return CompositeLight(pRenderGraph, depth, gtao, direct_lighting, width, height);
}

RenderGraphHandle LightingProcessor::CompositeLight(RenderGraph* pRenderGraph, RenderGraphHandle depth, RenderGraphHandle ao, RenderGraphHandle direct_lighting, uint32_t width, uint32_t height)
{
    struct CompositeLightData
    {
        RenderGraphHandle diffuseRT;
        RenderGraphHandle specularRT;
        RenderGraphHandle normalRT;
        RenderGraphHandle emissiveRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle ao;

        RenderGraphHandle directLighting;
        RenderGraphHandle indirectSpecular;
        RenderGraphHandle indirectDiffuse;

        RenderGraphHandle output;
    };

    auto pass = pRenderGraph->AddPass<CompositeLightData>("CompositeLight",
        [&](CompositeLightData& data, RenderGraphBuilder& builder)
        {
            BasePass* pBasePass = m_pRenderer->GetBassPass();

            data.diffuseRT = builder.Read(pBasePass->GetDiffuseRT(), GfxResourceState::ShaderResourceNonPS);
            data.specularRT = builder.Read(pBasePass->GetSpecularRT(), GfxResourceState::ShaderResourceNonPS);
            data.normalRT = builder.Read(pBasePass->GetNormalRT(), GfxResourceState::ShaderResourceNonPS);
            data.emissiveRT = builder.Read(pBasePass->GetEmissiveRT(), GfxResourceState::ShaderResourceNonPS);
            data.depthRT = builder.Read(depth, GfxResourceState::ShaderResourceNonPS);

            if (ao.IsValid())
            {
                data.ao = builder.Read(ao, GfxResourceState::ShaderResourceNonPS);
            }

            data.directLighting = builder.Read(direct_lighting, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageRenderTarget;
            data.output = builder.Create<RenderGraphTexture>(desc, "SceneColor RT");
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const CompositeLightData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* diffuseRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.diffuseRT);
            RenderGraphTexture* specularRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.specularRT);
            RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalRT);
            RenderGraphTexture* emissiveRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.emissiveRT);
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthRT);
            RenderGraphTexture* directLightingRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.directLighting);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            RenderGraphTexture* aoRT = nullptr;

            eastl::vector<eastl::string> defines;
            if (data.ao.IsValid())
            {
                aoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.ao);

                defines.push_back("GTAO=1");

                if (aoRT->GetTexture()->GetDesc().format == GfxFormat::R32UI)
                {
                    defines.push_back("GTSO=1");
                }
            }

            switch (m_pRenderer->GetOutputType())
            {
            case RendererOutput::Default:
                defines.push_back("OUTPUT_DEFAULT=1");
                break;
            case RendererOutput::Diffuse:
                defines.push_back("OUTPUT_DIFFUSE=1");
                break;
            case RendererOutput::Specular:
                defines.push_back("OUTPUT_SPECULAR=1");
                break;
            case RendererOutput::WorldNormal:
                defines.push_back("OUTPUT_WORLDNORMAL=1");
                break;
            case RendererOutput::Emissive:
                defines.push_back("OUTPUT_EMISSIVE=1");
                break;
            case RendererOutput::AO:
                defines.push_back("OUTPUT_AO=1");
                break;
            case RendererOutput::DirectLighting:
                defines.push_back("OUTPUT_DIRECT_LIGHTING=1");
                break;
            case RendererOutput::IndirectSpecular:
                defines.push_back("OUTPUT_INDIRECT_SPECULAR=1");
                break;
            case RendererOutput::IndirectDiffuse:
                defines.push_back("OUTPUT_INDIRECT_DIFFUSE=1");
                break;
            default:
                break;
            }

            GfxComputePipelineDesc psoDesc;
            psoDesc.cs = m_pRenderer->GetShader("composite_light.hlsl", "main", "cs_6_6", defines);
            IGfxPipelineState* pso = m_pRenderer->GetPipelineState(psoDesc, "CompositeLight PSO");

            pCommandList->SetPipelineState(pso);

            struct CB1
            {
                uint diffuseRT;
                uint specularRT;
                uint normalRT;
                uint emissiveRT;

                uint depthRT;
                uint directLightingRT;
                uint aoRT;
                uint outputRT;
            };

            CB1 cb1;
            cb1.diffuseRT = diffuseRT->GetSRV()->GetHeapIndex();
            cb1.specularRT = specularRT->GetSRV()->GetHeapIndex();
            cb1.normalRT = normalRT->GetSRV()->GetHeapIndex();
            cb1.emissiveRT = emissiveRT->GetSRV()->GetHeapIndex();
            cb1.depthRT = depthRT->GetSRV()->GetHeapIndex();
            cb1.directLightingRT = directLightingRT->GetSRV()->GetHeapIndex();
            if (aoRT)
            {
                cb1.aoRT = aoRT->GetSRV()->GetHeapIndex();
            }
            cb1.outputRT = outputRT->GetUAV()->GetHeapIndex();

            pCommandList->SetComputeConstants(1, &cb1, sizeof(cb1));

            pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });

    return pass->output;
}
