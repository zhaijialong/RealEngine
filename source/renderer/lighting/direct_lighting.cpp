#include "direct_lighting.h"
#include "clustered_light_lists.h"
#include "tiled_light_trees.h"
#include "restir_di.h"
#include "../renderer.h"
#include "utils/gui_util.h"

DirectLighting::DirectLighting(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pClusteredLightLists = eastl::make_unique<ClusteredLightLists>(pRenderer);
    m_pTiledLightTrees = eastl::make_unique<TiledLightTrees>(pRenderer);
    m_pReSTIRDI = eastl::make_unique<ReSTIRDI>(pRenderer);
}

DirectLighting::~DirectLighting()
{
}

void DirectLighting::OnGui()
{
    if (ImGui::CollapsingHeader("Direct Lighting"))
    {
        ImGui::Combo("Mode##DirectLighting", (int*)&m_mode, "Clustered\0TiledTree\0Hybrid\0ReSTIR\0\0", (int)DirectLightingMode::Num);
    }
}

RGHandle DirectLighting::AddPass(RenderGraph* pRenderGraph, RGHandle diffuse, RGHandle specular, RGHandle normal, 
    RGHandle customData, RGHandle depth, RGHandle shadow, uint32_t width, uint32_t height)
{
    switch (m_mode)
    {
    case DirectLightingMode::Clustered:
        m_pClusteredLightLists->Build(width, height);
        break;
    case DirectLightingMode::TiledTree:
        //todo
        break;
    case DirectLightingMode::Hybrid:
        //todo
        break;
    case DirectLightingMode::ReSTIR:
        //todo
        break;
    default:
        break;
    }

    struct DirectLightingData
    {
        RGHandle diffuseRT;
        RGHandle specularRT;
        RGHandle normalRT;
        RGHandle customDataRT;
        RGHandle depthRT;
        RGHandle shadow;
        RGHandle output;
    };

    auto lighting = pRenderGraph->AddPass<DirectLightingData>("Direct Lighting", RenderPassType::Compute,
        [&](DirectLightingData& data, RGBuilder& builder)
        {
            data.diffuseRT = builder.Read(diffuse);
            data.specularRT = builder.Read(specular);
            data.normalRT = builder.Read(normal);
            data.customDataRT = builder.Read(customData);
            data.depthRT = builder.Read(depth);
            data.shadow = builder.Read(shadow);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "Direct Lighting");
            data.output = builder.Write(data.output);
        },
        [=](const DirectLightingData& data, IGfxCommandList* pCommandList)
        {
            Render(pCommandList,
                pRenderGraph->GetTexture(data.diffuseRT), 
                pRenderGraph->GetTexture(data.specularRT),
                pRenderGraph->GetTexture(data.normalRT),
                pRenderGraph->GetTexture(data.customDataRT),
                pRenderGraph->GetTexture(data.depthRT),
                pRenderGraph->GetTexture(data.shadow),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    return lighting->output;
}

void DirectLighting::Render(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal,
    RGTexture* customData, RGTexture* depth, RGTexture* shadow, RGTexture* output, uint32_t width, uint32_t height)
{
    uint cb[] =
    {
        diffuse->GetSRV()->GetHeapIndex(),
        specular->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        customData->GetSRV()->GetHeapIndex(),
        depth->GetSRV()->GetHeapIndex(),
        shadow->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex()
    };

    eastl::vector<eastl::string> defines;

    switch (m_mode)
    {
    case DirectLightingMode::Clustered:
        defines.push_back("CLUSTERED_SHADING=1");
        break;
    default:
        break;
    }

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = m_pRenderer->GetShader("direct_lighting.hlsl", "main", GfxShaderType::CS, defines);
    IGfxPipelineState* pPSO = m_pRenderer->GetPipelineState(psoDesc, "direct lighting PSO");

    pCommandList->SetPipelineState(pPSO);
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
