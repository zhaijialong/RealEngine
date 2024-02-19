#include "ray_traced_shadow.h"
#include "../renderer.h"
#include "utils/gui_util.h"

RTShadow::RTShadow(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pDenoiser = eastl::make_unique<ShadowDenoiser>(pRenderer);

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/ray_trace.hlsl", "main", "cs_6_6", {});
    m_pRaytracePSO = pRenderer->GetPipelineState(psoDesc, "RTShadow PSO");
}

RGHandle RTShadow::AddPass(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, RGHandle velocityRT, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Shadow", [&]()
        {
            if (ImGui::Checkbox("Enable Denoiser##RTShadow", &m_bEnableDenoiser))
            {
                m_pDenoiser->InvalidateHistory();
            }
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "RTShadow");

    struct RTShadowData
    {
        RGHandle depth;
        RGHandle normal;
        RGHandle shadow;
    };

    auto rtshadow_pass = pRenderGraph->AddPass<RTShadowData>("RTShadow raytrace", 
        m_pRenderer->IsAsyncComputeEnabled() ? RenderPassType::AsyncCompute : RenderPassType::Compute,
        [&](RTShadowData& data, RGBuilder& builder)
        {
            data.depth = builder.Read(depthRT);
            data.normal = builder.Read(normalRT);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R8UNORM;
            data.shadow = builder.Create<RGTexture>(desc, "RTShadow raytraced shadow");
            data.shadow = builder.Write(data.shadow);
        },
        [=](const RTShadowData& data, IGfxCommandList* pCommandList)
        {
            RayTrace(pCommandList, 
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.shadow),
                width, height);
        });

    if (!m_bEnableDenoiser)
    {
        return rtshadow_pass->shadow;
    }
    
    return m_pDenoiser->AddPass(pRenderGraph, rtshadow_pass->shadow, depthRT, normalRT, velocityRT, width, height);
}

void RTShadow::RayTrace(IGfxCommandList* pCommandList, RGTexture* depthSRV, RGTexture* normalSRV, RGTexture* shadowUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pRaytracePSO);

    uint constants[3] = { depthSRV->GetSRV()->GetHeapIndex(), normalSRV->GetSRV()->GetHeapIndex(), shadowUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

