#include "shadow_denoiser.h"
#include "../renderer.h"
#include "fmt/format.h"

ShadowDenoiser::ShadowDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/denoiser_prepare.hlsl", "main", "cs_6_6", {});
    m_pPrepareMaskPSO = pRenderer->GetPipelineState(psoDesc, "ShadowDenoiser PSO");

    psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/denoiser_tileclassification.hlsl", "main", "cs_6_6", { "INVERTED_DEPTH_RANGE=1" });
    m_pTileClassificationPSO = pRenderer->GetPipelineState(psoDesc, "ShadowDenoiser PSO");

    for (int i = 0; i < 3; ++i)
    {
        eastl::string define = fmt::format("FILTER_PASS={}", i).c_str();
        psoDesc.cs = pRenderer->GetShader("ray_traced_shadow/denoiser_filter.hlsl", "main", "cs_6_6", { define });
        m_pFilterPSO[i] = pRenderer->GetPipelineState(psoDesc, "ShadowDenoiser PSO");
    }
}

RGHandle ShadowDenoiser::Render(RenderGraph* pRenderGraph, RGHandle input, RGHandle depthRT, RGHandle normalRT, RGHandle velocityRT, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "ShadowDenoiser");

    if (m_pMomentsTexture == nullptr ||
        m_pMomentsTexture->GetTexture()->GetDesc().width != width ||
        m_pMomentsTexture->GetTexture()->GetDesc().height != height)
    {
        m_pMomentsTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "ShadowDenoiser moments 0"));
        m_pPrevMomentsTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "ShadowDenoiser moments 1"));
        m_pHistoryTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "ShadowDenoiser history"));
        m_bHistoryInvalid = true;
    }

    eastl::swap(m_pMomentsTexture, m_pPrevMomentsTexture);

    struct DenoiserPreparePassData
    {
        RGHandle raytraceResult;
        RGHandle shadowMaskBuffer;
    };

    auto prepare_pass = pRenderGraph->AddPass<DenoiserPreparePassData>("ShadowDenoiser prepare", RenderPassType::Compute,
        [&](DenoiserPreparePassData& data, RGBuilder& builder)
        {
            data.raytraceResult = builder.Read(input);

            uint32_t tile_x = (width + 7) / 8;
            uint32_t tile_y = (height + 3) / 4;

            RGBuffer::Desc desc;
            desc.stride = sizeof(uint32_t);
            desc.size = tile_x * tile_y * sizeof(uint32_t);
            desc.usage = GfxBufferUsageStructuredBuffer;
            data.shadowMaskBuffer = builder.Create<RGBuffer>(desc, "ShadowDenoiser mask");
            data.shadowMaskBuffer = builder.Write(data.shadowMaskBuffer);
        },
        [=](const DenoiserPreparePassData& data, IGfxCommandList* pCommandList)
        {
            Prepare(pCommandList, 
                pRenderGraph->GetTexture(data.raytraceResult),
                pRenderGraph->GetBuffer(data.shadowMaskBuffer),
                width, height);
        });

    struct DenoiserTileClassificationData
    {
        RGHandle shadowMaskBuffer; //output of prepare pass
        RGHandle depthTexture;
        RGHandle normalTexture;
        RGHandle velocityTexture;
        RGHandle historyTexture;
        RGHandle prevLinearDepthTexture;
        RGHandle prevMomentsTexture;
        RGHandle momentsTexture; //RWTexture2D<float3>, r11g11b10f
        RGHandle tileMetaDataBuffer; //RWStructuredBuffer<uint>
        RGHandle reprojectionResultTexture; //RWTexture2D<float2>, r16g16f
    };

    auto classification_pass = pRenderGraph->AddPass<DenoiserTileClassificationData>("ShadowDenoiser tile classification", RenderPassType::Compute,
        [&](DenoiserTileClassificationData& data, RGBuilder& builder)
        {
            RGHandle momentsTexture = builder.Import(m_pMomentsTexture->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
            RGHandle prevMomentsTexture = builder.Import(m_pPrevMomentsTexture->GetTexture(), GfxResourceState::UnorderedAccess);
            RGHandle historyTexture = builder.Import(m_pHistoryTexture->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);

            data.shadowMaskBuffer = builder.Read(prepare_pass->shadowMaskBuffer);
            data.depthTexture = builder.Read(depthRT);
            data.normalTexture = builder.Read(normalRT);
            data.velocityTexture = builder.Read(velocityRT);
            data.historyTexture = builder.Read(historyTexture);
            data.prevLinearDepthTexture = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevMomentsTexture = builder.Read(prevMomentsTexture);

            data.momentsTexture = builder.Write(momentsTexture);

            uint32_t tile_x = (width + 7) / 8;
            uint32_t tile_y = (height + 3) / 4;

            RGBuffer::Desc bufferDesc;
            bufferDesc.stride = sizeof(uint32_t);
            bufferDesc.size = tile_x * tile_y * sizeof(uint32_t);
            bufferDesc.usage = GfxBufferUsageStructuredBuffer;
            data.tileMetaDataBuffer = builder.Create<RGBuffer>(bufferDesc, "ShadowDenoiser tile metadata");
            data.tileMetaDataBuffer = builder.Write(data.tileMetaDataBuffer);

            RGTexture::Desc textureDesc;
            textureDesc.width = width;
            textureDesc.height = height;
            textureDesc.format = GfxFormat::RG16F;
            data.reprojectionResultTexture = builder.Create<RGTexture>(textureDesc, "ShadowDenoiser reprojection result");
            data.reprojectionResultTexture = builder.Write(data.reprojectionResultTexture);
        },
        [=](const DenoiserTileClassificationData& data, IGfxCommandList* pCommandList)
        {
            TileClassification(pCommandList,
                pRenderGraph->GetBuffer(data.shadowMaskBuffer), 
                pRenderGraph->GetTexture(data.depthTexture),
                pRenderGraph->GetTexture(data.normalTexture),
                pRenderGraph->GetTexture(data.velocityTexture),
                pRenderGraph->GetBuffer(data.tileMetaDataBuffer),
                pRenderGraph->GetTexture(data.reprojectionResultTexture),
                width, height);
        });

    struct DenoiserFilterPassData
    {
        RGHandle inputTexture;
        RGHandle outputTexture;
        RGHandle depthTexture;
        RGHandle normalTexture;
        RGHandle tileMetaDataBuffer;
    };

    auto filter_pass0 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter0", RenderPassType::Compute,
        [&](DenoiserFilterPassData& data, RGBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT);
            data.normalTexture = builder.Read(normalRT);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer);
            data.inputTexture = builder.Read(classification_pass->reprojectionResultTexture);
            data.outputTexture = builder.Write(classification_pass->historyTexture);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            Filter(pCommandList, 
                pRenderGraph->GetTexture(data.inputTexture)->GetSRV(), 
                m_pHistoryTexture->GetUAV(),
                pRenderGraph->GetTexture(data.depthTexture),
                pRenderGraph->GetTexture(data.normalTexture),
                pRenderGraph->GetBuffer(data.tileMetaDataBuffer),
                0, width, height);
        });

    auto filter_pass1 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter1", RenderPassType::Compute,
        [&](DenoiserFilterPassData& data, RGBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT);
            data.normalTexture = builder.Read(normalRT);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer);
            data.inputTexture = builder.Read(filter_pass0->outputTexture);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RG16F;
            data.outputTexture = builder.Create<RGTexture>(desc, "ShadowDenoiser filter1 output");
            data.outputTexture = builder.Write(data.outputTexture);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            Filter(pCommandList, 
                m_pHistoryTexture->GetSRV(), 
                pRenderGraph->GetTexture(data.outputTexture)->GetUAV(),
                pRenderGraph->GetTexture(data.depthTexture),
                pRenderGraph->GetTexture(data.normalTexture),
                pRenderGraph->GetBuffer(data.tileMetaDataBuffer),
                1, width, height);
        });

    auto filter_pass2 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter2", RenderPassType::Compute,
        [&](DenoiserFilterPassData& data, RGBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT);
            data.normalTexture = builder.Read(normalRT);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer);
            data.inputTexture = builder.Read(filter_pass1->outputTexture);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R8UNORM;
            data.outputTexture = builder.Create<RGTexture>(desc, "ShadowDenoiser filter2 output");
            data.outputTexture = builder.Write(data.outputTexture);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            Filter(pCommandList,
                pRenderGraph->GetTexture(data.inputTexture)->GetSRV(),
                pRenderGraph->GetTexture(data.outputTexture)->GetUAV(),
                pRenderGraph->GetTexture(data.depthTexture),
                pRenderGraph->GetTexture(data.normalTexture),
                pRenderGraph->GetBuffer(data.tileMetaDataBuffer), 
                2, width, height);
        });

    return filter_pass2->outputTexture;
}

void ShadowDenoiser::Prepare(IGfxCommandList* pCommandList, RGTexture* raytraceResult, RGBuffer* maskBuffer, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPrepareMaskPSO);

    uint constants[2] = { raytraceResult->GetSRV()->GetHeapIndex(), maskBuffer->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 31) / 32, (height + 15) / 16, 1);
}

void ShadowDenoiser::TileClassification(IGfxCommandList* pCommandList, RGBuffer* shadowMaskBuffer, RGTexture* depthTexture, RGTexture* normalTexture,
    RGTexture* velocityTexture, RGBuffer* tileMetaDataBufferUAV, RGTexture* reprojectionResultTextureUAV, uint32_t width, uint32_t height)
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
    cb.shadowMaskBuffer = shadowMaskBuffer->GetSRV()->GetHeapIndex();
    cb.depthTexture = depthTexture->GetSRV()->GetHeapIndex();
    cb.normalTexture = normalTexture->GetSRV()->GetHeapIndex();
    cb.velocityTexture = velocityTexture->GetSRV()->GetHeapIndex();
    cb.historyTexture = m_pHistoryTexture->GetSRV()->GetHeapIndex();
    cb.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.prevMomentsTexture = m_pPrevMomentsTexture->GetSRV()->GetHeapIndex();
    cb.momentsTexture = m_pMomentsTexture->GetUAV()->GetHeapIndex();
    cb.tileMetaDataBuffer = tileMetaDataBufferUAV->GetUAV()->GetHeapIndex();
    cb.reprojectionResultTexture = reprojectionResultTextureUAV->GetUAV()->GetHeapIndex();
    cb.bFirstFrame = 0;

    if (m_bHistoryInvalid)
    {
        cb.bFirstFrame = 1;
        m_bHistoryInvalid = false;
    }

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void ShadowDenoiser::Filter(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, RGTexture* depthTexture, RGTexture* normalTexture,
    RGBuffer* tileMetaDataBuffer, uint32_t pass_index, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pFilterPSO[pass_index]);

    uint32_t constants[5] = {
        input->GetHeapIndex(),
        output->GetHeapIndex(),
        depthTexture->GetSRV()->GetHeapIndex(),
        normalTexture->GetSRV()->GetHeapIndex(),
        tileMetaDataBuffer->GetSRV()->GetHeapIndex(),
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}