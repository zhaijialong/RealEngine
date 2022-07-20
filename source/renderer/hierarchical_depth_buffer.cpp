#include "hierarchical_depth_buffer.h"
#include "renderer.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

HZB::HZB(Renderer* pRenderer) :
    m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_reprojection", "cs_6_6", {});
    m_pDepthReprojectionPSO = pRenderer->GetPipelineState(desc, "HZB depth reprojection PSO");

    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_dilation", "cs_6_6", {});
    m_pDepthDilationPSO = pRenderer->GetPipelineState(desc, "HZB depth dilation PSO");

    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "init_hzb", "cs_6_6", {});
    m_pInitHZBPSO = pRenderer->GetPipelineState(desc, "HZB init PSO");

    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "init_scene_hzb", "cs_6_6", {});
    m_pInitSceneHZBPSO = pRenderer->GetPipelineState(desc, "HZB init PSO");

    desc.cs = pRenderer->GetShader("hzb.hlsl", "build_hzb", "cs_6_6", {});
    m_pDepthMipFilterPSO = pRenderer->GetPipelineState(desc, "HZB generate mips PSO");

    desc.cs = pRenderer->GetShader("hzb.hlsl", "build_hzb", "cs_6_6", { "MIN_MAX_FILTER=1" });
    m_pDepthMipFilterMinMaxPSO = pRenderer->GetPipelineState(desc, "HZB generate mips PSO");

    m_pSPDCounterBuffer.reset(pRenderer->CreateTypedBuffer(nullptr, GfxFormat::R32UI, 1, "HZB::m_pSPDCounterBuffer", GfxMemoryType::GpuOnly, true));
}

void HZB::Generate1stPhaseCullingHZB(RenderGraph* graph)
{
    RENDER_GRAPH_EVENT(graph, "HZB");

    CalcHZBSize();

    struct DepthReprojectionData
    {
        RenderGraphHandle prevLinearDepth;
        RenderGraphHandle reprojectedDepth;
    };

    auto reprojection_pass = graph->AddPass<DepthReprojectionData>("Depth Reprojection", RenderPassType::Compute,
        [&](DepthReprojectionData& data, RenderGraphBuilder& builder)
        {
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle());

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.format = GfxFormat::R16F;
            data.reprojectedDepth = builder.Create<RenderGraphTexture>(desc, "Reprojected Depth RT");

            data.reprojectedDepth = builder.Write(data.reprojectedDepth);
        },
        [=](const DepthReprojectionData& data, IGfxCommandList* pCommandList)
        {
            //RenderGraphTexture* prevLinearDepth = (RenderGraphTexture*)graph->GetResource(data.prevLinearDepth);
            RenderGraphTexture* reprojectedDepth = (RenderGraphTexture*)graph->GetResource(data.reprojectedDepth);
            ReprojectDepth(pCommandList, m_pRenderer->GetPrevLinearDepthTexture()->GetSRV(), reprojectedDepth->GetTexture(), reprojectedDepth->GetUAV());
        });

    struct DepthDilationData
    {
        RenderGraphHandle reprojectedDepth;
        RenderGraphHandle dilatedDepth;
    };

    RenderGraphHandle hzb;

    auto dilation_pass = graph->AddPass<DepthDilationData>("Depth Dilation", RenderPassType::Compute,
        [&](DepthDilationData& data, RenderGraphBuilder& builder)
        {
            data.reprojectedDepth = builder.Read(reprojection_pass->reprojectedDepth);

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.mip_levels = m_nHZBMipCount;
            desc.format = GfxFormat::R16F;
            hzb = builder.Create<RenderGraphTexture>(desc, "1st phase HZB");

            data.dilatedDepth = builder.Write(hzb);
        },
        [=](const DepthDilationData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* reprojectedDepth = (RenderGraphTexture*)graph->GetResource(data.reprojectedDepth);
            RenderGraphTexture* dilatedDepth = (RenderGraphTexture*)graph->GetResource(data.dilatedDepth);
            DilateDepth(pCommandList, reprojectedDepth->GetSRV(), dilatedDepth->GetUAV());
        });

    struct BuildHZBData
    {
        RenderGraphHandle hzb;
    };

    auto hzb_pass = graph->AddPass<BuildHZBData>("Build HZB", RenderPassType::Compute,
        [&](BuildHZBData& data, RenderGraphBuilder& builder)
        {
            data.hzb = builder.Read(dilation_pass->dilatedDepth);

            m_1stPhaseCullingHZBMips[0] = data.hzb;
            for (uint32_t i = 1; i < m_nHZBMipCount; ++i)
            {
                m_1stPhaseCullingHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IGfxCommandList* pCommandList)
        {
            ResetCounterBuffer(pCommandList);

            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);
            BuildHZB(pCommandList, hzb);
        });
}

void HZB::Generate2ndPhaseCullingHZB(RenderGraph* graph, RenderGraphHandle depthRT)
{
    RENDER_GRAPH_EVENT(graph, "HZB");

    struct InitHZBData
    {
        RenderGraphHandle inputDepthRT;
        RenderGraphHandle hzb;
    };

    RenderGraphHandle hzb;

    auto init_hzb = graph->AddPass<InitHZBData>("Init HZB", RenderPassType::Compute,
        [&](InitHZBData& data, RenderGraphBuilder& builder)
        {
            data.inputDepthRT = builder.Read(depthRT);

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.mip_levels = m_nHZBMipCount;
            desc.format = GfxFormat::R16F;
            hzb = builder.Create<RenderGraphTexture>(desc, "2nd phase HZB");

            data.hzb = builder.Write(hzb);
        },
        [=](const InitHZBData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputDepth = (RenderGraphTexture*)graph->GetResource(data.inputDepthRT);
            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);
            InitHZB(pCommandList, inputDepth->GetSRV(), hzb->GetUAV());
        });

    struct BuildHZBData
    {
        RenderGraphHandle hzb;
    };

    auto hzb_pass = graph->AddPass<BuildHZBData>("Build HZB", RenderPassType::Compute,
        [&](BuildHZBData& data, RenderGraphBuilder& builder)
        {
            data.hzb = builder.Read(init_hzb->hzb);

            m_2ndPhaseCullingHZBMips[0] = data.hzb;
            for (uint32_t i = 1; i < m_nHZBMipCount; ++i)
            {
                m_2ndPhaseCullingHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);
            BuildHZB(pCommandList, hzb);
        });
}

void HZB::GenerateSceneHZB(RenderGraph* graph, RenderGraphHandle depthRT)
{
    RENDER_GRAPH_EVENT(graph, "HZB");

    struct InitHZBData
    {
        RenderGraphHandle inputDepthRT;
        RenderGraphHandle hzb;
    };

    RenderGraphHandle hzb;

    auto init_hzb = graph->AddPass<InitHZBData>("Init HZB", RenderPassType::Compute,
        [&](InitHZBData& data, RenderGraphBuilder& builder)
        {
            data.inputDepthRT = builder.Read(depthRT);

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.mip_levels = m_nHZBMipCount;
            desc.format = GfxFormat::RG16F;
            hzb = builder.Create<RenderGraphTexture>(desc, "SceneHZB");

            data.hzb = builder.Write(hzb);
        },
        [=](const InitHZBData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputDepth = (RenderGraphTexture*)graph->GetResource(data.inputDepthRT);
            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);

            InitHZB(pCommandList, inputDepth->GetSRV(), hzb->GetUAV(), true);
        });

    struct BuildHZBData
    {
        RenderGraphHandle hzb;
    };

    auto hzb_pass = graph->AddPass<BuildHZBData>("Build HZB", RenderPassType::Compute,
        [&](BuildHZBData& data, RenderGraphBuilder& builder)
        {
            data.hzb = builder.Read(init_hzb->hzb);

            m_sceneHZBMips[0] = data.hzb;
            for (uint32_t i = 1; i < m_nHZBMipCount; ++i)
            {
                m_sceneHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);
            BuildHZB(pCommandList, hzb, true);
        });
}

RenderGraphHandle HZB::Get1stPhaseCullingHZBMip(uint32_t mip) const
{
    RE_ASSERT(mip < m_nHZBMipCount);
    return m_1stPhaseCullingHZBMips[mip];
}

RenderGraphHandle HZB::Get2ndPhaseCullingHZBMip(uint32_t mip) const
{
    RE_ASSERT(mip < m_nHZBMipCount);
    return m_2ndPhaseCullingHZBMips[mip];
}

RenderGraphHandle HZB::GetSceneHZBMip(uint32_t mip) const
{
    RE_ASSERT(mip < m_nHZBMipCount);
    return m_sceneHZBMips[mip];
}

void HZB::CalcHZBSize()
{
    uint32_t mipsX = (uint32_t)eastl::max(ceilf(log2f((float)m_pRenderer->GetRenderWidth())), 1.0f);
    uint32_t mipsY = (uint32_t)eastl::max(ceilf(log2f((float)m_pRenderer->GetRenderHeight())), 1.0f);

    m_nHZBMipCount = eastl::max(mipsX, mipsY);
    RE_ASSERT(m_nHZBMipCount <= MAX_HZB_MIP_COUNT);

    m_hzbSize.x = 1 << (mipsX - 1);
    m_hzbSize.y = 1 << (mipsY - 1);
}

void HZB::ReprojectDepth(IGfxCommandList* pCommandList, IGfxDescriptor* prevLinearDepthSRV, IGfxTexture* reprojectedDepthTexture, IGfxDescriptor* reprojectedDepthUAV)
{
    float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCommandList->ClearUAV(reprojectedDepthTexture, reprojectedDepthUAV, clear_value);
    pCommandList->UavBarrier(reprojectedDepthTexture);

    pCommandList->SetPipelineState(m_pDepthReprojectionPSO);

    uint32_t root_consts[4] = { prevLinearDepthSRV->GetHeapIndex(), reprojectedDepthUAV->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->Dispatch((m_hzbSize.x + 7) / 8, (m_hzbSize.y + 7) / 8, 1);
}

void HZB::DilateDepth(IGfxCommandList* pCommandList, IGfxDescriptor* reprojectedDepthSRV, IGfxDescriptor* hzbMip0UAV)
{
    pCommandList->SetPipelineState(m_pDepthDilationPSO);

    uint32_t root_consts[4] = { reprojectedDepthSRV->GetHeapIndex(), hzbMip0UAV->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->Dispatch((m_hzbSize.x + 7) / 8, (m_hzbSize.y + 7) / 8, 1);
}

void HZB::BuildHZB(IGfxCommandList* pCommandList, RenderGraphTexture* texture, bool min_max)
{
    pCommandList->SetPipelineState(min_max ? m_pDepthMipFilterMinMaxPSO : m_pDepthMipFilterPSO);

    const GfxTextureDesc& textureDesc = texture->GetTexture()->GetDesc();

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, textureDesc.width, textureDesc.height); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, textureDesc.mip_levels - 1);

    struct spdConstants
    {
        uint mips;
        uint numWorkGroups;
        uint2 workGroupOffset;

        float2 invInputSize;
        uint c_imgSrc;
        uint c_spdGlobalAtomicUAV;

        //hlsl packing rules : every element in an array is stored in a four-component vector
        uint4 c_imgDst[12]; // do no access MIP [5]
    };

    spdConstants constants = {};
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.mips = numWorkGroupsAndMips[1];
    constants.workGroupOffset[0] = workGroupOffset[0];
    constants.workGroupOffset[1] = workGroupOffset[1];
    constants.invInputSize[0] = 1.0f / textureDesc.width;
    constants.invInputSize[1] = 1.0f / textureDesc.height;

    constants.c_imgSrc = texture->GetSRV()->GetHeapIndex();
    constants.c_spdGlobalAtomicUAV = m_pSPDCounterBuffer->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < textureDesc.mip_levels - 1; ++i)
    {
        constants.c_imgDst[i].x = texture->GetUAV(i + 1, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 1; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void HZB::InitHZB(IGfxCommandList* pCommandList, IGfxDescriptor* inputDepthSRV, IGfxDescriptor* hzbMip0UAV, bool min_max)
{
    pCommandList->SetPipelineState(min_max ? m_pInitSceneHZBPSO : m_pInitHZBPSO);

    uint32_t root_consts[4] = { inputDepthSRV->GetHeapIndex(), hzbMip0UAV->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->Dispatch((m_hzbSize.x + 7) / 8, (m_hzbSize.y + 7) / 8, 1);
}

void HZB::ResetCounterBuffer(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "HZB reset counter");

    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopyDst);
    pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);
    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);
}
