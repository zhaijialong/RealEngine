#include "hzb_occlusion_culling.h"
#include "renderer.h"

HZBOcclusionCulling::HZBOcclusionCulling(Renderer* pRenderer) :
    m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_reprojection", "cs_6_6", {});
    m_pDepthReprojectionPSO = pRenderer->GetPipelineState(desc, "HZBOcclusionCulling depth reprojection PSO");

    desc.cs = pRenderer->GetShader("hzb_reprojection.hlsl", "depth_dilation", "cs_6_6", {});
    m_pDepthDilationPSO = pRenderer->GetPipelineState(desc, "HZBOcclusionCulling depth dilation PSO");
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
            data.dilatedDepth = builder.Create<RenderGraphTexture>(desc, "Reprojected HZB");

            data.dilatedDepth = builder.Write(data.dilatedDepth, GfxResourceState::UnorderedAccess);
        },
        [=](const DepthDilationData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* reprojectedDepth = (RenderGraphTexture*)graph->GetResource(data.reprojectedDepth);
            RenderGraphTexture* dilatedDepth = (RenderGraphTexture*)graph->GetResource(data.dilatedDepth);
            DilateDepth(pCommandList, reprojectedDepth->GetSRV(), dilatedDepth->GetUAV());
        });

    m_hzb = dilation_pass->dilatedDepth;
}

void HZBOcclusionCulling::CalcHZBSize()
{
    uint32_t mipsX = (uint32_t)std::max(ceilf(log2f((float)m_pRenderer->GetBackbufferWidth())) - 1, 1.0f);
    uint32_t mipsY = (uint32_t)std::max(ceilf(log2f((float)m_pRenderer->GetBackbufferHeight())) - 1, 1.0f);

    m_nHZBMipCount = std::max(mipsX, mipsY);

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
