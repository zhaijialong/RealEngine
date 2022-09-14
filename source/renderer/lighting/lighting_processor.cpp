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
    m_pReSTIRGI = eastl::make_unique<ReSTIRGI>(pRenderer);
}

RGHandle LightingProcessor::Render(RenderGraph* pRenderGraph, RGHandle depth, RGHandle linear_depth, RGHandle velocity, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Lighting");

    BasePass* pBasePass = m_pRenderer->GetBassPass();
    RGHandle diffuse = pBasePass->GetDiffuseRT();
    RGHandle specular = pBasePass->GetSpecularRT();
    RGHandle normal = pBasePass->GetNormalRT();
    RGHandle emissive = pBasePass->GetEmissiveRT();
    RGHandle customData = pBasePass->GetCustomDataRT();

    RGHandle half_normal_depth = ExtractHalfDepthNormal(pRenderGraph, depth, normal, width, height);

    RGHandle gtao = m_pGTAO->Render(pRenderGraph, depth, normal, width, height);
    RGHandle shadow = m_pRTShdow->Render(pRenderGraph, depth, normal, velocity, width, height);
    RGHandle direct_lighting = m_pClusteredShading->Render(pRenderGraph, diffuse, specular, normal, customData, depth, shadow, width, height);
    RGHandle indirect_specular = m_pReflection->Render(pRenderGraph, depth, linear_depth, normal, velocity, width, height);
    RGHandle indirect_diffuse = m_pReSTIRGI->Render(pRenderGraph, half_normal_depth, depth, linear_depth, normal, velocity, width, height);

    return CompositeLight(pRenderGraph, depth, gtao, direct_lighting, indirect_specular, indirect_diffuse, width, height);
}

RGHandle LightingProcessor::CompositeLight(RenderGraph* pRenderGraph, RGHandle depth, RGHandle ao, RGHandle direct_lighting,
    RGHandle indirect_specular, RGHandle indirect_diffuse, uint32_t width, uint32_t height)
{
    struct CompositeLightData
    {
        RGHandle diffuseRT;
        RGHandle specularRT;
        RGHandle normalRT;
        RGHandle emissiveRT;
        RGHandle customDataRT;
        RGHandle depthRT;
        RGHandle ao;

        RGHandle directLighting;
        RGHandle indirectSpecular;
        RGHandle indirectDiffuse;

        RGHandle output;
    };

    auto pass = pRenderGraph->AddPass<CompositeLightData>("CompositeLight", RenderPassType::Compute,
        [&](CompositeLightData& data, RGBuilder& builder)
        {
            BasePass* pBasePass = m_pRenderer->GetBassPass();

            data.diffuseRT = builder.Read(pBasePass->GetDiffuseRT());
            data.specularRT = builder.Read(pBasePass->GetSpecularRT());
            data.normalRT = builder.Read(pBasePass->GetNormalRT());
            data.emissiveRT = builder.Read(pBasePass->GetEmissiveRT());
            data.customDataRT = builder.Read(pBasePass->GetCustomDataRT());
            data.depthRT = builder.Read(depth);
            data.directLighting = builder.Read(direct_lighting);

            if (ao.IsValid())
            {
                data.ao = builder.Read(ao);
            }

            if (indirect_specular.IsValid())
            {
                data.indirectSpecular = builder.Read(indirect_specular);
            }

            if (indirect_diffuse.IsValid())
            {
                data.indirectDiffuse = builder.Read(indirect_diffuse);
            }

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "SceneColor RT");
            data.output = builder.Write(data.output);
        },
        [=](const CompositeLightData& data, IGfxCommandList* pCommandList)
        {
            RGTexture* diffuseRT = pRenderGraph->GetTexture(data.diffuseRT);
            RGTexture* specularRT = pRenderGraph->GetTexture(data.specularRT);
            RGTexture* normalRT = pRenderGraph->GetTexture(data.normalRT);
            RGTexture* emissiveRT = pRenderGraph->GetTexture(data.emissiveRT);
            RGTexture* customDataRT = pRenderGraph->GetTexture(data.customDataRT);
            RGTexture* depthRT = pRenderGraph->GetTexture(data.depthRT);
            RGTexture* directLightingRT = pRenderGraph->GetTexture(data.directLighting);
            RGTexture* outputRT = pRenderGraph->GetTexture(data.output);
            RGTexture* aoRT = nullptr;
            RGTexture* indirectSpecularRT = nullptr;
            RGTexture* indirectDiffuseRT = nullptr;

            eastl::vector<eastl::string> defines;
            if (data.ao.IsValid())
            {
                aoRT = pRenderGraph->GetTexture(data.ao);

                defines.push_back("GTAO=1");

                if (aoRT->GetTexture()->GetDesc().format == GfxFormat::R32UI)
                {
                    defines.push_back("GTSO=1");
                }
            }

            if (data.indirectSpecular.IsValid())
            {
                indirectSpecularRT = pRenderGraph->GetTexture(data.indirectSpecular);
                defines.push_back("SPECULAR_GI=1");
            }

            if (data.indirectDiffuse.IsValid())
            {
                indirectDiffuseRT = pRenderGraph->GetTexture(data.indirectDiffuse);
                defines.push_back("DIFFUSE_GI=1");
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
            case RendererOutput::Roughness:
                defines.push_back("OUTPUT_ROUGHNESS=1");
                break;
            case RendererOutput::Emissive:
                defines.push_back("OUTPUT_EMISSIVE=1");
                break;
            case RendererOutput::ShadingModel:
                defines.push_back("OUTPUT_SHADING_MODEL=1");
                break;
            case RendererOutput::CustomData:
                defines.push_back("OUTPUT_CUSTOM_DATA=1");
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
                uint indirectSprcularRT;

                uint indirectDiffuseRT;
                uint customDataRT;
                float hsrMaxRoughness;
                uint outputRT;
            };

            CB1 cb1;
            cb1.diffuseRT = diffuseRT->GetSRV()->GetHeapIndex();
            cb1.specularRT = specularRT->GetSRV()->GetHeapIndex();
            cb1.normalRT = normalRT->GetSRV()->GetHeapIndex();
            cb1.emissiveRT = emissiveRT->GetSRV()->GetHeapIndex();
            cb1.customDataRT = customDataRT->GetSRV()->GetHeapIndex();
            cb1.depthRT = depthRT->GetSRV()->GetHeapIndex();
            cb1.directLightingRT = directLightingRT->GetSRV()->GetHeapIndex();
            if (aoRT)
            {
                cb1.aoRT = aoRT->GetSRV()->GetHeapIndex();
            }
            if (indirectSpecularRT)
            {
                cb1.indirectSprcularRT = indirectSpecularRT->IsImported() ? m_pReflection->GetOutputRadianceSRV()->GetHeapIndex() : indirectSpecularRT->GetSRV()->GetHeapIndex();
            }
            if (indirectDiffuseRT)
            {
                cb1.indirectDiffuseRT = indirectDiffuseRT->IsImported() ? m_pReSTIRGI->GetOutputIrradianceSRV()->GetHeapIndex() : indirectDiffuseRT->GetSRV()->GetHeapIndex();
            }
            cb1.hsrMaxRoughness = m_pReflection->GetMaxRoughness();
            cb1.outputRT = outputRT->GetUAV()->GetHeapIndex();

            pCommandList->SetComputeConstants(1, &cb1, sizeof(cb1));

            pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });

    return pass->output;
}

RGHandle LightingProcessor::ExtractHalfDepthNormal(RenderGraph* pRenderGraph, RGHandle depth, RGHandle normal, uint32_t width, uint32_t height)
{
    uint32_t half_width = (width + 1) / 2;
    uint32_t half_height = (height + 1) / 2;

    struct ExtractHalfDepthNormalData
    {
        RGHandle depth;
        RGHandle normal;
        RGHandle output;
    };

    return pRenderGraph->AddPass<ExtractHalfDepthNormalData>("Extract half depth/normal", RenderPassType::Compute,
        [&](ExtractHalfDepthNormalData& data, RGBuilder& builder)
        {
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            RGTexture::Desc desc;
            desc.width = half_width;
            desc.height = half_height;
            desc.format = GfxFormat::RG32UI;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "Half Depth/Normal"));
        },
        [=](const ExtractHalfDepthNormalData& data, IGfxCommandList* pCommandList)
        {
            static IGfxPipelineState* pso = nullptr;
            if (pso == nullptr)
            {
                GfxComputePipelineDesc psoDesc;
                psoDesc.cs = m_pRenderer->GetShader("extract_half_depth_normal.hlsl", "main", "cs_6_6", {});
                pso = m_pRenderer->GetPipelineState(psoDesc, "extract half depth/normal PSO");
            }

            pCommandList->SetPipelineState(pso);

            uint32_t constants[3] = {
                pRenderGraph->GetTexture(data.depth)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.normal)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
            };

            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->Dispatch((half_width + 7) / 8, (half_height + 7) / 8, 1);
        })->output;
}

