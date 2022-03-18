#include "ray_traced_shadow.h"
#include "../renderer.h"
#include "utils/gui_util.h"

RTShadow::RTShadow(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/ray_trace.hlsl", "main", "cs_6_6", {});
    m_pRaytracePSO = pRenderer->GetPipelineState(psoDesc, "RTShadow PSO");

    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/denoiser_prepare.hlsl", "main", "cs_6_6", {});
    m_pPrepareMaskPSO = pRenderer->GetPipelineState(psoDesc, "RTShadow Denoiser PSO");
}

RenderGraphHandle RTShadow::Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, uint32_t width, uint32_t height)
{
    GUI("Lighting", "Shadow", [&]()
        {
            ImGui::Checkbox("Enable Denoiser##RTShadow", &m_bEnableDenoiser);
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "RTShadow");

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

    if (!m_bEnableDenoiser)
    {
        rtshadow_pass->shadow;
    }

    struct DenoiserPreparePassData
    {
        RenderGraphHandle raytraceResult;
        RenderGraphHandle shadowMaskBuffer;
    };

    auto prepare_pass = pRenderGraph->AddPass<DenoiserPreparePassData>("ShadowDenoiser prepare",
        [&](DenoiserPreparePassData& data, RenderGraphBuilder& builder)
        {
            data.raytraceResult = builder.Read(rtshadow_pass->shadow, GfxResourceState::ShaderResourceNonPS);

            uint32_t tile_x = (width + 7) / 8;
            uint32_t tile_y = (height + 3) / 4;

            RenderGraphBuffer::Desc desc;
            desc.stride = sizeof(uint32_t);
            desc.size = tile_x * tile_y * sizeof(uint32_t);
            desc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.shadowMaskBuffer = builder.Create<RenderGraphBuffer>(desc, "ShadowDenoiser mask");
            data.shadowMaskBuffer = builder.Write(data.shadowMaskBuffer, GfxResourceState::UnorderedAccess);

            builder.MakeTarget(); //todo
        },
        [=](const DenoiserPreparePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* raytraceResult = (RenderGraphTexture*)pRenderGraph->GetResource(data.raytraceResult);
            RenderGraphBuffer* shadowMaskBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.shadowMaskBuffer);

            DenoiserPrepare(pCommandList, raytraceResult->GetSRV(), shadowMaskBuffer->GetUAV(), width, height);
        });

    return rtshadow_pass->shadow;
}

void RTShadow::RayTrace(IGfxCommandList* pCommandList, IGfxDescriptor* depthSRV, IGfxDescriptor* normalSRV, IGfxDescriptor* shadowUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pRaytracePSO);

    uint constants[3] = { depthSRV->GetHeapIndex(), normalSRV->GetHeapIndex(), shadowUAV->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void RTShadow::DenoiserPrepare(IGfxCommandList* pCommandList, IGfxDescriptor* raytraceResult, IGfxDescriptor* maskBuffer, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPrepareMaskPSO);

    uint constants[2] = { raytraceResult->GetHeapIndex(), maskBuffer->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 31) / 32, (height + 15) / 16, 1);
}
