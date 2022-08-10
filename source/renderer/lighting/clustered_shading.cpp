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

RGHandle ClusteredShading::Render(RenderGraph* pRenderGraph, RGHandle diffuse, RGHandle specular, RGHandle normal, 
    RGHandle customData, RGHandle depth, RGHandle shadow, uint32_t width, uint32_t height)
{
    struct ClusteredShadingData
    {
        RGHandle diffuseRT;
        RGHandle specularRT;
        RGHandle normalRT;
        RGHandle customDataRT;
        RGHandle depthRT;
        RGHandle shadow;
        RGHandle output;
    };

    auto clustered_shading = pRenderGraph->AddPass<ClusteredShadingData>("Clustered Shading", RenderPassType::Compute,
        [&](ClusteredShadingData& data, RGBuilder& builder)
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
        [=](const ClusteredShadingData& data, IGfxCommandList* pCommandList)
        {
            Draw(pCommandList, 
                pRenderGraph->GetTexture(data.diffuseRT), 
                pRenderGraph->GetTexture(data.specularRT),
                pRenderGraph->GetTexture(data.normalRT),
                pRenderGraph->GetTexture(data.customDataRT),
                pRenderGraph->GetTexture(data.depthRT),
                pRenderGraph->GetTexture(data.shadow),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    return clustered_shading->output;
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal, 
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
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
