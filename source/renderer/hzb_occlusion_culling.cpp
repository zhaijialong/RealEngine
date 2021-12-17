#include "hzb_occlusion_culling.h"
#include "renderer.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

HZBOcclusionCulling::HZBOcclusionCulling(Renderer* pRenderer) :
    m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_reprojection", "cs_6_6", {});
    m_pDepthReprojectionPSO = pRenderer->GetPipelineState(desc, "HZBOcclusionCulling depth reprojection PSO");

    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_dilation", "cs_6_6", {});
    m_pDepthDilationPSO = pRenderer->GetPipelineState(desc, "HZBOcclusionCulling depth dilation PSO");

    desc.cs = pRenderer->GetShader("hzb.hlsl", "build_hzb", "cs_6_6", {});
    m_pDepthMipFilterPSO = pRenderer->GetPipelineState(desc, "HZBOcclusionCulling build HZB PSO");

    m_pSPDCounterBuffer.reset(pRenderer->CreateTypedBuffer(nullptr, GfxFormat::R32UI, 1, "HZBOcclusionCulling::m_pSPDCounterBuffer", GfxMemoryType::GpuOnly, true));
}

void HZBOcclusionCulling::GenerateHZB(RenderGraph* graph)
{
    CalcHZBSize();

    struct DepthReprojectionData
    {
        RenderGraphHandle prevLinearDepth;
        RenderGraphHandle reprojectedDepth;
    };

    auto reprojection_pass = graph->AddPass<DepthReprojectionData>("Depth Reprojection",
        [&](DepthReprojectionData& data, RenderGraphBuilder& builder)
        {
            data.prevLinearDepth = builder.Read(m_pRenderer->GetPrevLinearDepthHandle(), GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.format = GfxFormat::R16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.reprojectedDepth = builder.Create<RenderGraphTexture>(desc, "Reprojected Depth RT");

            data.reprojectedDepth = builder.Write(data.reprojectedDepth, GfxResourceState::UnorderedAccess);
        },
        [=](const DepthReprojectionData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* prevLinearDepth = (RenderGraphTexture*)graph->GetResource(data.prevLinearDepth);
            RenderGraphTexture* reprojectedDepth = (RenderGraphTexture*)graph->GetResource(data.reprojectedDepth);
            ReprojectDepth(pCommandList, prevLinearDepth->GetSRV(), reprojectedDepth->GetUAV());
        });

    struct DepthDilationData
    {
        RenderGraphHandle reprojectedDepth;
        RenderGraphHandle dilatedDepth;
    };

    RenderGraphHandle hzb;

    auto dilation_pass = graph->AddPass<DepthDilationData>("Depth Dilation",
        [&](DepthDilationData& data, RenderGraphBuilder& builder)
        {
            data.reprojectedDepth = builder.Read(reprojection_pass->reprojectedDepth, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_hzbSize.x;
            desc.height = m_hzbSize.y;
            desc.mip_levels = m_nHZBMipCount;
            desc.format = GfxFormat::R16F;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            hzb = builder.Create<RenderGraphTexture>(desc, "Reprojected HZB");

            data.dilatedDepth = builder.Write(hzb, GfxResourceState::UnorderedAccess, 0);
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

    auto hzb_pass = graph->AddPass<BuildHZBData>("Build HZB",
        [&](BuildHZBData& data, RenderGraphBuilder& builder)
        {
            data.hzb = builder.Read(dilation_pass->dilatedDepth, GfxResourceState::ShaderResourceNonPS);

            m_hzbMips[0] = data.hzb;
            for (uint32_t i = 1; i < m_nHZBMipCount; ++i)
            {
                m_hzbMips[i] = builder.Write(hzb, GfxResourceState::UnorderedAccess, i);
            }
        },
        [=](const BuildHZBData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* hzb = (RenderGraphTexture*)graph->GetResource(data.hzb);
            BuildHZB(pCommandList, hzb);
        });
}

RenderGraphHandle HZBOcclusionCulling::GetHZBMip(uint32_t mip) const
{
    RE_ASSERT(mip < m_nHZBMipCount);
    return m_hzbMips[mip];
}

void HZBOcclusionCulling::CalcHZBSize()
{
    uint32_t mipsX = (uint32_t)std::max(ceilf(log2f((float)m_pRenderer->GetBackbufferWidth())) - 1, 1.0f);
    uint32_t mipsY = (uint32_t)std::max(ceilf(log2f((float)m_pRenderer->GetBackbufferHeight())) - 1, 1.0f);

    m_nHZBMipCount = std::max(mipsX, mipsY);
    RE_ASSERT(m_nHZBMipCount <= MAX_HZB_MIP_COUNT);

    m_hzbSize.x = 1 << mipsX;
    m_hzbSize.y = 1 << mipsY;
}

void HZBOcclusionCulling::ReprojectDepth(IGfxCommandList* pCommandList, IGfxDescriptor* prevLinearDepthSRV, IGfxDescriptor* reprojectedDepthUAV)
{
    pCommandList->SetPipelineState(m_pDepthReprojectionPSO);

    uint32_t root_consts[4] = { prevLinearDepthSRV->GetHeapIndex(), reprojectedDepthUAV->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->Dispatch((m_hzbSize.x + 7) / 8, (m_hzbSize.y + 7) / 8, 1);
}

void HZBOcclusionCulling::DilateDepth(IGfxCommandList* pCommandList, IGfxDescriptor* reprojectedDepthSRV, IGfxDescriptor* hzbMip0UAV)
{
    pCommandList->SetPipelineState(m_pDepthDilationPSO);

    uint32_t root_consts[4] = { reprojectedDepthSRV->GetHeapIndex(), hzbMip0UAV->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));

    pCommandList->Dispatch((m_hzbSize.x + 7) / 8, (m_hzbSize.y + 7) / 8, 1);
}

void HZBOcclusionCulling::BuildHZB(IGfxCommandList* pCommandList, RenderGraphTexture* texture)
{
    ResetCounterBuffer(pCommandList);

    pCommandList->SetPipelineState(m_pDepthMipFilterPSO);

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, m_hzbSize.x, m_hzbSize.y); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, m_nHZBMipCount - 1);

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
    constants.invInputSize[0] = 1.0f / m_hzbSize.x;
    constants.invInputSize[1] = 1.0f / m_hzbSize.y;

    constants.c_imgSrc = texture->GetSRV()->GetHeapIndex();
    constants.c_spdGlobalAtomicUAV = m_pSPDCounterBuffer->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < m_nHZBMipCount - 1; ++i)
    {
        constants.c_imgDst[i].x = texture->GetUAV(i + 1, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 1; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void HZBOcclusionCulling::ResetCounterBuffer(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "HZB reset counter");

    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopyDst);
    pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);
    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);
}
