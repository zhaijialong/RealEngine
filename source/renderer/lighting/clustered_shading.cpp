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

RenderGraphHandle ClusteredShading::Render(RenderGraph* pRenderGraph, RenderGraphHandle diffuse, RenderGraphHandle specular, RenderGraphHandle normal, 
    RenderGraphHandle customData, RenderGraphHandle depth, RenderGraphHandle shadow, uint32_t width, uint32_t height)
{
    struct ClusteredShadingData
    {
        RenderGraphHandle diffuseRT;
        RenderGraphHandle specularRT;
        RenderGraphHandle normalRT;
        RenderGraphHandle customDataRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle shadow;
        RenderGraphHandle output;
    };

    auto clustered_shading = pRenderGraph->AddPass<ClusteredShadingData>("Clustered Shading", RenderPassType::Compute,
        [&](ClusteredShadingData& data, RenderGraphBuilder& builder)
        {
            data.diffuseRT = builder.Read(diffuse);
            data.specularRT = builder.Read(specular);
            data.normalRT = builder.Read(normal);
            data.customDataRT = builder.Read(customData);
            data.depthRT = builder.Read(depth);
            data.shadow = builder.Read(shadow);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "Direct Lighting");
            data.output = builder.Write(data.output);
        },
        [=](const ClusteredShadingData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* diffuseRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.diffuseRT);
            RenderGraphTexture* specularRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.specularRT);
            RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalRT);
            RenderGraphTexture* customDataRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.customDataRT);
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthRT);
            RenderGraphTexture* shadowRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.shadow);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Draw(pCommandList, diffuseRT->GetSRV(), specularRT->GetSRV(), normalRT->GetSRV(), customDataRT->GetSRV(), depthRT->GetSRV(), shadowRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    return clustered_shading->output;
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* diffuse, IGfxDescriptor* specular, IGfxDescriptor* normal, 
    IGfxDescriptor* customData, IGfxDescriptor* depth, IGfxDescriptor* shadow, IGfxDescriptor* output, uint32_t width, uint32_t height)
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
    cb.diffuseRT = diffuse->GetHeapIndex();
    cb.specularRT = specular->GetHeapIndex();
    cb.normalRT = normal->GetHeapIndex();
    cb.customDataRT = customData->GetHeapIndex();
    cb.depthRT = depth->GetHeapIndex();
    cb.shadowRT = shadow->GetHeapIndex();
    cb.outputRT = output->GetHeapIndex();
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
