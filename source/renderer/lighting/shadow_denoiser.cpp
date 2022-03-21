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

RenderGraphHandle ShadowDenoiser::Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depthRT, RenderGraphHandle normalRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height)
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
        RenderGraphHandle raytraceResult;
        RenderGraphHandle shadowMaskBuffer;
    };

    auto prepare_pass = pRenderGraph->AddPass<DenoiserPreparePassData>("ShadowDenoiser prepare",
        [&](DenoiserPreparePassData& data, RenderGraphBuilder& builder)
        {
            data.raytraceResult = builder.Read(input, GfxResourceState::ShaderResourceNonPS);

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

            Prepare(pCommandList, raytraceResult->GetSRV(), shadowMaskBuffer->GetUAV(), width, height);
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
            RenderGraphHandle historyTexture = builder.Import(m_pHistoryTexture->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);

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
        },
        [=](const DenoiserTileClassificationData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphBuffer* shadowMaskBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.shadowMaskBuffer);
            RenderGraphTexture* depthTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthTexture);
            RenderGraphTexture* normalTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalTexture);
            RenderGraphTexture* velocityTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocityTexture);
            RenderGraphBuffer* tileMetaDataBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileMetaDataBuffer);
            RenderGraphTexture* reprojectionResultTexture = (RenderGraphTexture*)pRenderGraph->GetResource(data.reprojectionResultTexture);

            TileClassification(pCommandList, shadowMaskBuffer->GetSRV(), depthTexture->GetSRV(), normalTexture->GetSRV(), velocityTexture->GetSRV(),
                tileMetaDataBuffer->GetUAV(), reprojectionResultTexture->GetUAV(), width, height);
        });

    struct DenoiserFilterPassData
    {
        RenderGraphHandle inputTexture;
        RenderGraphHandle outputTexture;
        RenderGraphHandle depthTexture;
        RenderGraphHandle normalTexture;
        RenderGraphHandle tileMetaDataBuffer;
    };

    auto filter_pass0 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter0",
        [&](DenoiserFilterPassData& data, RenderGraphBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.normalTexture = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer, GfxResourceState::ShaderResourceNonPS);
            data.inputTexture = builder.Read(classification_pass->reprojectionResultTexture, GfxResourceState::ShaderResourceNonPS);
            data.outputTexture = builder.Write(classification_pass->historyTexture, GfxResourceState::UnorderedAccess);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputTexture);
            IGfxDescriptor* outputUAV = m_pHistoryTexture->GetUAV();
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthTexture);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalTexture);
            RenderGraphBuffer* tileMetaData = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileMetaDataBuffer);

            Filter(pCommandList, input->GetSRV(), outputUAV, depth->GetSRV(), normal->GetSRV(), tileMetaData->GetSRV(), 0, width, height);
        });

    auto filter_pass1 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter1",
        [&](DenoiserFilterPassData& data, RenderGraphBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.normalTexture = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer, GfxResourceState::ShaderResourceNonPS);
            data.inputTexture = builder.Read(filter_pass0->outputTexture, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RG16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputTexture = builder.Create<RenderGraphTexture>(desc, "ShadowDenoiser filter1 output");
            data.outputTexture = builder.Write(data.outputTexture, GfxResourceState::UnorderedAccess);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            IGfxDescriptor* input = m_pHistoryTexture->GetSRV();
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputTexture);
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthTexture);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalTexture);
            RenderGraphBuffer* tileMetaData = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileMetaDataBuffer);

            Filter(pCommandList, input, output->GetUAV(), depth->GetSRV(), normal->GetSRV(), tileMetaData->GetSRV(), 1, width, height);
        });

    auto filter_pass2 = pRenderGraph->AddPass<DenoiserFilterPassData>("ShadowDenoiser filter2",
        [&](DenoiserFilterPassData& data, RenderGraphBuilder& builder)
        {
            data.depthTexture = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);
            data.normalTexture = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);
            data.tileMetaDataBuffer = builder.Read(classification_pass->tileMetaDataBuffer, GfxResourceState::ShaderResourceNonPS);
            data.inputTexture = builder.Read(filter_pass1->outputTexture, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R8UNORM;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputTexture = builder.Create<RenderGraphTexture>(desc, "ShadowDenoiser filter2 output");
            data.outputTexture = builder.Write(data.outputTexture, GfxResourceState::UnorderedAccess);
        },
        [=](const DenoiserFilterPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputTexture);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputTexture);
            RenderGraphTexture* depth = (RenderGraphTexture*)pRenderGraph->GetResource(data.depthTexture);
            RenderGraphTexture* normal = (RenderGraphTexture*)pRenderGraph->GetResource(data.normalTexture);
            RenderGraphBuffer* tileMetaData = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileMetaDataBuffer);

            Filter(pCommandList, input->GetSRV(), output->GetUAV(), depth->GetSRV(), normal->GetSRV(), tileMetaData->GetSRV(), 2, width, height);
        });

    return filter_pass2->outputTexture;
}

void ShadowDenoiser::Prepare(IGfxCommandList* pCommandList, IGfxDescriptor* raytraceResult, IGfxDescriptor* maskBuffer, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPrepareMaskPSO);

    uint constants[2] = { raytraceResult->GetHeapIndex(), maskBuffer->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 31) / 32, (height + 15) / 16, 1);
}

void ShadowDenoiser::TileClassification(IGfxCommandList* pCommandList, IGfxDescriptor* shadowMaskBuffer, IGfxDescriptor* depthTexture, IGfxDescriptor* normalTexture,
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
}

void ShadowDenoiser::Filter(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, IGfxDescriptor* depthTexture, IGfxDescriptor* normalTexture, IGfxDescriptor* tileMetaDataBuffer, uint32_t pass_index, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pFilterPSO[pass_index]);

    uint32_t constants[5] = {
        input->GetHeapIndex(),
        output->GetHeapIndex(),
        depthTexture->GetHeapIndex(),
        normalTexture->GetHeapIndex(),
        tileMetaDataBuffer->GetHeapIndex(),
    };
    pCommandList->SetComputeConstants(0, constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}