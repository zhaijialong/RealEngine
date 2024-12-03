#include "direct_lighting.h"
#include "clustered_light_lists.h"
#include "tiled_light_trees.h"
#include "restir_di.h"
#include "../renderer.h"

DirectLighting::DirectLighting(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pClusteredLightLists = eastl::make_unique<ClusteredLightLists>(pRenderer);
    m_pTiledLightTrees = eastl::make_unique<TiledLightTrees>(pRenderer);
    m_pReSTIRDI = eastl::make_unique<ReSTIRDI>(pRenderer);

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("direct_lighting.hlsl", "main", GfxShaderType::CS);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "direct lighting PSO");
}

DirectLighting::~DirectLighting()
{
}

RGHandle DirectLighting::AddPass(RenderGraph* pRenderGraph, RGHandle diffuse, RGHandle specular, RGHandle normal, 
    RGHandle customData, RGHandle depth, RGHandle shadow, uint32_t width, uint32_t height)
{
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
    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint diffuseRT;
        uint specularRT;
        uint normalRT;
        uint customDataRT;
        uint depthRT;
        uint shadowRT;
        uint outputRT;
    };

    CB cb;
    cb.diffuseRT = diffuse->GetSRV()->GetHeapIndex();
    cb.specularRT = specular->GetSRV()->GetHeapIndex();
    cb.normalRT = normal->GetSRV()->GetHeapIndex();
    cb.customDataRT = customData->GetSRV()->GetHeapIndex();
    cb.depthRT = depth->GetSRV()->GetHeapIndex();
    cb.shadowRT = shadow->GetSRV()->GetHeapIndex();
    cb.outputRT = output->GetUAV()->GetHeapIndex();
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
