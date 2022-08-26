#include "gi_denoiser.h"
#include "../renderer.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

GIDenoiser::GIDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("recurrent_blur/pre_blur.hlsl", "main", "cs_6_6", {});
    m_pPreBlurPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/pre blur PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/temporal_accumulation.hlsl", "main", "cs_6_6", {});
    m_pTemporalAccumulationPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/temporal accumulation PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/mip_generation.hlsl", "main", "cs_6_6", {});
    m_pMipGenerationPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/mip generation PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/mip_generation_depth.hlsl", "main", "cs_6_6", {});
    m_pMipGenerationDepthPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/depth mip generation PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/history_reconstruction.hlsl", "main", "cs_6_6", {});
    m_pHistoryReconstructionPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/history reconstruction PSO");

    desc.cs = pRenderer->GetShader("recurrent_blur/blur.hlsl", "main", "cs_6_6", {});
    m_pBlurPSO = pRenderer->GetPipelineState(desc, "GIDenoiser/blur PSO");
}

void GIDenoiser::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pTemporalAccumulationSH == nullptr ||
        m_pTemporalAccumulationSH->GetTexture()->GetDesc().width != width ||
        m_pTemporalAccumulationSH->GetTexture()->GetDesc().height != height)
    {
        m_pTemporalAccumulationSH.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA32UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation historySH"));
        m_pTemporalAccumulationCount0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation count 0"));
        m_pTemporalAccumulationCount1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UI, GfxTextureUsageUnorderedAccess, "GIDenoiser/temporal accumulation count 1"));

        m_pHistoryRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "GIDenoiser/history radiance"));

        m_bHistoryInvalid = true;
    }

    m_historyRadiance = pRenderGraph->Import(m_pHistoryRadiance->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiser::Render(RenderGraph* pRenderGraph, RGHandle radianceSH, RGHandle variance, 
    RGHandle depth, RGHandle linear_depth, RGHandle normal, RGHandle velocity, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "GI Denoiser");

    struct PreBlurData
    {
        RGHandle inputSH;
        RGHandle variance;
        RGHandle depth;
        RGHandle normal;
        RGHandle outputSH;
    };

    auto pre_blur = pRenderGraph->AddPass<PreBlurData>("GI Denoiser - pre blur", RenderPassType::Compute,
        [&](PreBlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(radianceSH);
            data.variance = builder.Read(variance);
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/pre blur outputSH"));
        },
        [=](const PreBlurData& data, IGfxCommandList* pCommandList)
        {
            PreBlur(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.variance),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.outputSH),
                width, height);
        });

    eastl::swap(m_pTemporalAccumulationCount0, m_pTemporalAccumulationCount1);

    struct TemporalAccumulationData
    {
        RGHandle inputSH;
        RGHandle historySH;
        RGHandle historyAccumulationCount;
        RGHandle depth;
        RGHandle normal;
        RGHandle velocity;
        RGHandle prevLinearDepth;
        RGHandle prevNormal;
        RGHandle outputSH;
        RGHandle outputAccumulationCount;
    };

    auto temporal_accumulation = pRenderGraph->AddPass<TemporalAccumulationData>("GI Denoiser - temporal accumulation", RenderPassType::Compute,
        [&](TemporalAccumulationData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(pre_blur->outputSH);
            data.historySH = builder.Read(builder.Import(m_pTemporalAccumulationSH->GetTexture(), GfxResourceState::UnorderedAccess));
            data.historyAccumulationCount = builder.Read(builder.Import(m_pTemporalAccumulationCount0->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.velocity = builder.Read(velocity);
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());
            data.prevNormal = builder.Read(m_pRenderer->GetPrevNormalHandle());

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA32UI;
            data.outputSH = builder.Write(builder.Create<RGTexture>(desc, "GI Denoiser/temporal accumulation outputSH"));

            data.outputAccumulationCount = builder.Write(builder.Import(m_pTemporalAccumulationCount1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
        },
        [=](const TemporalAccumulationData& data, IGfxCommandList* pCommandList)
        {
            TemporalAccumulation(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.outputSH),
                width, height);
        });

    struct MipGenerationData
    {
        RGHandle input;
        RGHandle outputMips1;
        RGHandle outputMips2;
        RGHandle outputMips3;
        RGHandle outputMips4;
    };

    auto sh_mip_generation = pRenderGraph->AddPass<MipGenerationData>("GI Denoiser - mip generation", RenderPassType::Compute,
        [&](MipGenerationData& data, RGBuilder& builder)
        {
            data.input = builder.Read(temporal_accumulation->outputSH);

            RGTexture::Desc desc;
            desc.width = (width + 1) / 2;
            desc.height = (height + 1) / 2;
            desc.mip_levels = 4;
            desc.format = GfxFormat::RGBA32UI;
            RGHandle shMips = builder.Create<RGTexture>(desc, "GI Denoiser/SH mips");

            data.outputMips1 = builder.Write(shMips, 0);
            data.outputMips2 = builder.Write(shMips, 1);
            data.outputMips3 = builder.Write(shMips, 2);
            data.outputMips4 = builder.Write(shMips, 3);
        },
        [=](const MipGenerationData& data, IGfxCommandList* pCommandList)
        {
            MipGeneration(pCommandList,
                pRenderGraph->GetTexture(data.input),
                pRenderGraph->GetTexture(data.outputMips1),
                width, height, false);
        });

    auto depth_mip_generation = pRenderGraph->AddPass<MipGenerationData>("GI Denoiser - depth mip generation", RenderPassType::Compute,
        [&](MipGenerationData& data, RGBuilder& builder)
        {
            data.input = builder.Read(linear_depth);

            RGTexture::Desc desc;
            desc.width = (width + 1) / 2;
            desc.height = (height + 1) / 2;
            desc.mip_levels = 4;
            desc.format = GfxFormat::R32F;
            RGHandle depthMips = builder.Create<RGTexture>(desc, "GI Denoiser/depth mips");

            data.outputMips1 = builder.Write(depthMips, 0);
            data.outputMips2 = builder.Write(depthMips, 1);
            data.outputMips3 = builder.Write(depthMips, 2);
            data.outputMips4 = builder.Write(depthMips, 3);
        },
        [=](const MipGenerationData& data, IGfxCommandList* pCommandList)
        {
            MipGeneration(pCommandList,
                pRenderGraph->GetTexture(data.input),
                pRenderGraph->GetTexture(data.outputMips1),
                width, height, true);
        });

    struct HistoryReconstructionData
    {
        RGHandle inputSHMips1;
        RGHandle inputSHMips2;
        RGHandle inputSHMips3;
        RGHandle inputSHMips4;
        
        RGHandle linearDepthMips1;
        RGHandle linearDepthMips2;
        RGHandle linearDepthMips3;
        RGHandle linearDepthMips4;

        RGHandle accumulationCount;
        RGHandle depth;
        RGHandle normal;
        RGHandle outputSH;
    };

    auto history_reconstruction = pRenderGraph->AddPass<HistoryReconstructionData>("GI Denoiser - history reconstruction", RenderPassType::Compute,
        [&](HistoryReconstructionData& data, RGBuilder& builder)
        {
            data.inputSHMips1 = builder.Read(sh_mip_generation->outputMips1, 0);
            data.inputSHMips2 = builder.Read(sh_mip_generation->outputMips2, 1);
            data.inputSHMips3 = builder.Read(sh_mip_generation->outputMips3, 2);
            data.inputSHMips4 = builder.Read(sh_mip_generation->outputMips4, 3);

            data.linearDepthMips1 = builder.Read(depth_mip_generation->outputMips1, 0);
            data.linearDepthMips2 = builder.Read(depth_mip_generation->outputMips2, 1);
            data.linearDepthMips3 = builder.Read(depth_mip_generation->outputMips3, 2);
            data.linearDepthMips4 = builder.Read(depth_mip_generation->outputMips4, 3);

            data.accumulationCount = builder.Read(temporal_accumulation->outputAccumulationCount);
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.outputSH = builder.Write(temporal_accumulation->outputSH);
        },
        [=](const HistoryReconstructionData& data, IGfxCommandList* pCommandList)
        {
            HistoryReconstruction(pCommandList,
                pRenderGraph->GetTexture(data.inputSHMips1),
                pRenderGraph->GetTexture(data.linearDepthMips1),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                pRenderGraph->GetTexture(data.outputSH),
                width, height);
        });

    struct BlurData
    {
        RGHandle inputSH;
        RGHandle accumulationCount;
        RGHandle depth;
        RGHandle normal;
        RGHandle outputSH;
        RGHandle outputRadiance;
    };

    auto blur = pRenderGraph->AddPass<BlurData>("GI Denoiser - blur", RenderPassType::Compute,
        [&](BlurData& data, RGBuilder& builder)
        {
            data.inputSH = builder.Read(history_reconstruction->outputSH);
            data.accumulationCount = builder.Read(temporal_accumulation->outputAccumulationCount);
            data.depth = builder.Read(depth);
            data.normal = builder.Read(normal);
            data.outputSH = builder.Write(temporal_accumulation->historySH);

            data.outputRadiance = builder.Write(pRenderGraph->Import(m_pHistoryRadiance->GetTexture(), GfxResourceState::ShaderResourceNonPS));
        },
        [=](const BlurData& data, IGfxCommandList* pCommandList)
        {
            Blur(pCommandList,
                pRenderGraph->GetTexture(data.inputSH),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.normal),
                width, height);
        });

    return blur->outputRadiance;
}

void GIDenoiser::PreBlur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* variance, RGTexture* depth, RGTexture* normal,
    RGTexture* outputSH, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPreBlurPSO);

    struct CB
    {
        uint inputSHTexture;
        uint varianceTexture;
        uint depthTexture;
        uint normalTexture;
        uint outputSHTexture;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.varianceTexture = variance->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = outputSH->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void GIDenoiser::TemporalAccumulation(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, RGTexture* velocity,
    RGTexture* outputSH, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pTemporalAccumulationPSO);

    struct CB
    {
        uint inputSHTexture;
        uint historySHTexture;
        uint historyAccumulationCountTexture;
        uint depthTexture;
        uint normalTexture;
        uint velocityTexture;
        uint prevLinearDepthTexture;
        uint prevNormalTexture;
        uint outputSHTexture;
        uint outputAccumulationCountTexture;
        uint bHistoryInvalid;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.historySHTexture = m_pTemporalAccumulationSH->GetSRV()->GetHeapIndex();
    constants.historyAccumulationCountTexture = m_pTemporalAccumulationCount0->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.velocityTexture = velocity->GetSRV()->GetHeapIndex();
    constants.prevLinearDepthTexture = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    constants.prevNormalTexture = m_pRenderer->GetPrevNormalTexture()->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = outputSH->GetUAV()->GetHeapIndex();
    constants.outputAccumulationCountTexture = m_pTemporalAccumulationCount1->GetUAV()->GetHeapIndex();
    constants.bHistoryInvalid = m_bHistoryInvalid;

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;
    }
}

void GIDenoiser::MipGeneration(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* outputMips, uint32_t width, uint32_t height, bool depth)
{
    pCommandList->SetPipelineState(depth ? m_pMipGenerationDepthPSO : m_pMipGenerationPSO);

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, width, height); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, 4);

    struct CB
    {
        uint mips;
        uint numWorkGroups;
        uint2 workGroupOffset;

        float2 invInputSize;
        uint inputTexture;
        uint spdGlobalAtomicUAV;

        //hlsl packing rules : every element in an array is stored in a four-component vector
        uint4 outputTexture[4];
    };

    CB constants;
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.mips = numWorkGroupsAndMips[1];
    constants.workGroupOffset[0] = workGroupOffset[0];
    constants.workGroupOffset[1] = workGroupOffset[1];
    constants.invInputSize[0] = 1.0f / width;
    constants.invInputSize[1] = 1.0f / height;

    constants.inputTexture = input->GetSRV()->GetHeapIndex();
    constants.spdGlobalAtomicUAV = m_pRenderer->GetSPDCounterBuffer()->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < 4; ++i)
    {
        constants.outputTexture[i].x = outputMips->GetUAV(i, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 1; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void GIDenoiser::HistoryReconstruction(IGfxCommandList* pCommandList, RGTexture* inputSHMips, RGTexture* linearDepthMips, RGTexture* depth, RGTexture* normal,
    RGTexture* outputSH, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pHistoryReconstructionPSO);

    struct CB
    {
        uint inputSHMipsTexture;
        uint linearDepthMipsTexture;
        uint depthTexture;
        uint normalTexture;
        uint accumulationCountTexture;
        uint outputSHTexture;
    };

    CB constants;
    constants.inputSHMipsTexture = inputSHMips->GetSRV()->GetHeapIndex();
    constants.linearDepthMipsTexture = linearDepthMips->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.accumulationCountTexture = m_pTemporalAccumulationCount1->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = outputSH->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void GIDenoiser::Blur(IGfxCommandList* pCommandList, RGTexture* inputSH, RGTexture* depth, RGTexture* normal, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pBlurPSO);

    struct CB
    {
        uint inputSHTexture;
        uint accumulationCountTexture;
        uint depthTexture;
        uint normalTexture;
        uint outputSHTexture;
        uint outputRadianceTexture;
    };

    CB constants;
    constants.inputSHTexture = inputSH->GetSRV()->GetHeapIndex();
    constants.accumulationCountTexture = m_pTemporalAccumulationCount1->GetSRV()->GetHeapIndex();
    constants.depthTexture = depth->GetSRV()->GetHeapIndex();
    constants.normalTexture = normal->GetSRV()->GetHeapIndex();
    constants.outputSHTexture = m_pTemporalAccumulationSH->GetUAV()->GetHeapIndex();
    constants.outputRadianceTexture = m_pHistoryRadiance->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
