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

    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/denoiser_tileclassification.hlsl", "main", "cs_6_6", { "INVERTED_DEPTH_RANGE=1" });
    m_pTileClassificationPSO = pRenderer->GetPipelineState(psoDesc, "RTShadow Denoiser PSO");
}

RenderGraphHandle RTShadow::Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height)
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
    
    if (m_pMomentsTexture == nullptr ||
        m_pMomentsTexture->GetTexture()->GetDesc().width != width ||
        m_pMomentsTexture->GetTexture()->GetDesc().height != height)
    {
        m_pMomentsTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "RTShadow moments 0"));
        m_pPrevMomentsTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "RTShadow moments 1"));
        m_pHistoryTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "RTShadow history"));
        m_bHistoryInvalid = true;
    }

    eastl::swap(m_pMomentsTexture, m_pPrevMomentsTexture);

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
        },
        [=](const DenoiserPreparePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* raytraceResult = (RenderGraphTexture*)pRenderGraph->GetResource(data.raytraceResult);
            RenderGraphBuffer* shadowMaskBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.shadowMaskBuffer);

            DenoiserPrepare(pCommandList, raytraceResult->GetSRV(), shadowMaskBuffer->GetUAV(), width, height);
        });

    struct DenoiserTileClassificationData
    {
        RenderGraphHandle shadowMaskBuffer; //output of prepare pass
        RenderGraphHandle depthTexture;
        RenderGraphHandle normalTexture;
        RenderGraphHandle velocityTexture;
        RenderGraphHandle historyTexture;
        RenderGraphHandle prevLinearDepthTexture;
        RenderGraphHandle prevMomentsTexture;
        RenderGraphHandle momentsTexture; //RWTexture2D<float3>, r11g11b10f
        RenderGraphHandle tileMetaDataBuffer; //RWStructuredBuffer<uint>
        RenderGraphHandle reprojectionResultTexture; //RWTexture2D<float2>, r16g16f
    };

    auto classification_pass = pRenderGraph->AddPass<DenoiserTileClassificationData>("ShadowDenoiser tile classification",
        [&](DenoiserTileClassificationData& data, RenderGraphBuilder& builder)
        {
            RenderGraphHandle momentsTexture = builder.Import(m_pMomentsTexture->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
            RenderGraphHandle prevMomentsTexture = builder.Import(m_pPrevMomentsTexture->GetTexture(), GfxResourceState::UnorderedAccess);
            RenderGraphHandle historyTexture = builder.Import(m_pHistoryTexture->GetTexture(), GfxResourceState::UnorderedAccess);

            data.shadowMaskBuffer = builder.Read(prepare_pass->shadowMaskBuffer, GfxResourceState::ShaderResourceNonPS);
            data.depthTexture = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.normalTexture = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);
            data.velocityTexture = builder.Read(velocityRT, GfxResourceState::ShaderResourceNonPS);
            data.historyTexture = builder.Read(historyTexture, GfxResourceState::ShaderResourceNonPS);
            data.prevLinearDepthTexture = builder.Read(m_pRenderer->GetPrevLinearDepthHandle(), GfxResourceState::ShaderResourceNonPS);
            data.prevMomentsTexture = builder.Read(prevMomentsTexture, GfxResourceState::ShaderResourceNonPS);

            data.momentsTexture = builder.Write(momentsTexture, GfxResourceState::UnorderedAccess);

            uint32_t tile_x = (width + 7) / 8;
            uint32_t tile_y = (height + 3) / 4;

            RenderGraphBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint32_t);
            bufferDesc.size = tile_x * tile_y * sizeof(uint32_t);
            bufferDesc.usage = GfxBufferUsageStructuredBuffer | GfxBufferUsageUnorderedAccess;
            data.tileMetaDataBuffer = builder.Create<RenderGraphBuffer>(bufferDesc, "ShadowDenoiser tile metadata");
            data.tileMetaDataBuffer = builder.Write(data.tileMetaDataBuffer, GfxResourceState::UnorderedAccess);

            RenderGraphTexture::Desc textureDesc;
            textureDesc.width = width;
            textureDesc.height = height;
            textureDesc.format = GfxFormat::RG16F;
            textureDesc.usage = GfxTextureUsageUnorderedAccess;
            data.reprojectionResultTexture = builder.Create<RenderGraphTexture>(textureDesc, "ShadowDenoiser reprojection result");
            data.reprojectionResultTexture = builder.Write(data.reprojectionResultTexture, GfxResourceState::UnorderedAccess);

            builder.MakeTarget(); //todo
        },
        [=](const DenoiserTileClassificationData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* shadowMaskBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.shadowMaskBuffer);
            RenderGraphTexture* depthTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthTexture);
            RenderGraphTexture* normalTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalTexture);
            RenderGraphTexture* velocityTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocityTexture);
            RenderGraphBuffer* tileMetaDataBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileMetaDataBuffer);
            RenderGraphTexture* reprojectionResultTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.reprojectionResultTexture);

            DenoiserTileClassification(pCommandList, shadowMaskBuffer->GetSRV(), depthTexture->GetSRV(), normalTexture->GetSRV(), velocityTexture->GetSRV(),
                tileMetaDataBuffer->GetUAV(), reprojectionResultTexture->GetUAV(), width, height);
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

void RTShadow::DenoiserTileClassification(IGfxCommandList* pCommandList, IGfxDescriptor* shadowMaskBuffer, IGfxDescriptor* depthTexture, IGfxDescriptor* normalTexture,
    IGfxDescriptor* velocityTexture, IGfxDescriptor* tileMetaDataBufferUAV, IGfxDescriptor* reprojectionResultTextureUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTileClassificationPSO);

    struct CB
    {
        uint shadowMaskBuffer; //output of prepare pass
        uint depthTexture;
        uint normalTexture;
        uint velocityTexture;
        uint historyTexture;
        uint prevLinearDepthTexture;
        uint prevMomentsTexture;
        uint momentsTexture; //RWTexture2D<float3>, r11g11b10f
        uint tileMetaDataBuffer; //RWStructuredBuffer<uint>
        uint reprojectionResultTexture; //RWTexture2D<float2>, r16g16f
        uint bFirstFrame;
    };

    CB cb;
    cb.shadowMaskBuffer = shadowMaskBuffer->GetHeapIndex();
    cb.depthTexture = depthTexture->GetHeapIndex();
    cb.normalTexture = normalTexture->GetHeapIndex();
    cb.velocityTexture = velocityTexture->GetHeapIndex();
    cb.historyTexture = m_pHistoryTexture->GetSRV()->GetHeapIndex();
    cb.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.prevMomentsTexture = m_pPrevMomentsTexture->GetSRV()->GetHeapIndex();
    cb.momentsTexture = m_pMomentsTexture->GetUAV()->GetHeapIndex();
    cb.tileMetaDataBuffer = tileMetaDataBufferUAV->GetHeapIndex();
    cb.reprojectionResultTexture = reprojectionResultTextureUAV->GetHeapIndex();
    cb.bFirstFrame = 0;

    if (m_bHistoryInvalid)
    {
        cb.bFirstFrame = 1;
        m_bHistoryInvalid = false;
    }

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    //todo : delete
    pCommandList->ResourceBarrier(m_pHistoryTexture->GetTexture(), 0, GfxResourceState::ShaderResourceNonPS, GfxResourceState::UnorderedAccess);
}
