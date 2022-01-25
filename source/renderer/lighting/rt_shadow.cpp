#include "rt_shadow.h"
#include "../renderer.h"

RTShadow::RTShadow(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("rt_shadow.hlsl", "raytrace_shadow", "cs_6_6", {});
    m_pRaytracePSO = pRenderer->GetPipelineState(psoDesc, "RTAO PSO");
}

RenderGraphHandle RTShadow::Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, uint32_t width, uint32_t height)
{
    struct RTShadowData
    {
        RenderGraphHandle depth;
        RenderGraphHandle normal;
        RenderGraphHandle shadow;
    };

    auto rtshadow_pass = pRenderGraph->AddPass<RTShadowData>("RTShadow raytrace",
        [&](RTShadowData& data, RenderGraphBuilder& builder)
        {
            data.depth = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.normal = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R8UNORM;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.shadow = builder.Create<RenderGraphTexture>(desc, "RTShadow raytraced shadow");
            data.shadow = builder.Write(data.shadow, GfxResourceState::UnorderedAccess);
        },
        [=](const RTShadowData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.normal);
            RenderGraphTexture* shadowRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.shadow);

            RayTrace(pCommandList, depthRT->GetSRV(), normalRT->GetSRV(), shadowRT->GetUAV(), width, height);
        });

    return rtshadow_pass->shadow;
}

void RTShadow::RayTrace(IGfxCommandList* pCommandList, IGfxDescriptor* depthSRV, IGfxDescriptor* normalSRV, IGfxDescriptor* shadowUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pRaytracePSO);

    uint root_constants[3] = { depthSRV->GetHeapIndex(), normalSRV->GetHeapIndex(), shadowUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, root_constants, sizeof(root_constants));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
