#include "gtao.h"
#include "../renderer.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "utils/gui_util.h"
#include "XeGTAO.h"

GTAO::GTAO(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    std::vector<std::string> defines;
    defines.push_back("XE_GTAO_USE_HALF_FLOAT_PRECISION=0");

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_prefilter_depth_16x16", "cs_6_6", defines);
    m_pPrefilterDepthPSO = pRenderer->GetPipelineState(psoDesc, "GTAO prefilter depth PSO");

    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_denoise", "cs_6_6", defines);
    m_pDenoisePSO = pRenderer->GetPipelineState(psoDesc, "GTAO denoise PSO");

    defines.push_back("QUALITY_LEVEL=0");
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
    m_pGTAOLowPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

    defines.back() = "QUALITY_LEVEL=1";
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
    m_pGTAOMediumPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

    defines.back() = "QUALITY_LEVEL=2";
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
    m_pGTAOHighPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

    defines.back() = "QUALITY_LEVEL=3";
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
    m_pGTAOUltraPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

    //PSOs with GTSO
    {
        defines.pop_back();
        defines.push_back("XE_GTAO_COMPUTE_BENT_NORMALS=1");

        psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_denoise", "cs_6_6", defines);
        m_pSODenoisePSO = pRenderer->GetPipelineState(psoDesc, "GTAO denoise PSO");

        defines.push_back("QUALITY_LEVEL=0");
        psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
        m_pGTAOSOLowPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

        defines.back() = "QUALITY_LEVEL=1";
        psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
        m_pGTAOSOMediumPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

        defines.back() = "QUALITY_LEVEL=2";
        psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
        m_pGTAOSOHighPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

        defines.back() = "QUALITY_LEVEL=3";
        psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
        m_pGTAOSOUltraPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");
    }

    CreateHilbertLUT();
}

RenderGraphHandle GTAO::Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, uint32_t width, uint32_t height)
{
    GUI("Lighting", "GTAO",
        [&]()
        {
            ImGui::Checkbox("Enable##GTAO", &m_bEnable);
            ImGui::Checkbox("Enable GTSO##GTAO", &m_bEnableGTSO);
            ImGui::Combo("Quality Level", &m_qualityLevel, "Low\0Medium\0High\0Ultra\00");
            ImGui::SliderFloat("Radius##GTAO", &m_radius, 0.0f, 5.0f, "%.1f");
        });

    if (!m_bEnable)
    {
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "GTAO");

    auto gtao_filter_depth_pass = pRenderGraph->AddPass<GTAOFilterDepthPassData>("GTAO filter depth",
        [&](GTAOFilterDepthPassData& data, RenderGraphBuilder& builder)
        {
            data.inputDepth = builder.Read(depthRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.mip_levels = 5;
            desc.format = GfxFormat::R16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            RenderGraphHandle gtao_depth = builder.Create<RenderGraphTexture>(desc, "GTAO depth(HZB)");

            data.outputDepthMip0 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 0);
            data.outputDepthMip1 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 1);
            data.outputDepthMip2 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 2);
            data.outputDepthMip3 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 3);
            data.outputDepthMip4 = builder.Write(gtao_depth, GfxResourceState::UnorderedAccess, 4);
        },
        [=](const GTAOFilterDepthPassData& data, IGfxCommandList* pCommandList)
        {
            FilterDepth(pCommandList, data, width, height);
        });

    auto gtao_pass = pRenderGraph->AddPass<GTAOPassData>("GTAO",
        [&](GTAOPassData& data, RenderGraphBuilder& builder)
        {
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip0, GfxResourceState::ShaderResourceNonPS, 0);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip1, GfxResourceState::ShaderResourceNonPS, 1);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip2, GfxResourceState::ShaderResourceNonPS, 2);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip3, GfxResourceState::ShaderResourceNonPS, 3);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip4, GfxResourceState::ShaderResourceNonPS, 4);

            data.inputNormal = builder.Read(normalRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = m_bEnableGTSO ? GfxFormat::R32UI : GfxFormat::R8UI;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputAOTerm = builder.Create<RenderGraphTexture>(desc, "GTAO AOTerm");
            data.outputAOTerm = builder.Write(data.outputAOTerm, GfxResourceState::UnorderedAccess);

            desc.format = GfxFormat::R8UNORM;
            data.outputEdge = builder.Create<RenderGraphTexture>(desc, "GTAO Edge");
            data.outputEdge = builder.Write(data.outputEdge, GfxResourceState::UnorderedAccess);
        },
        [=](const GTAOPassData& data, IGfxCommandList* pCommandList)
        {
            Draw(pCommandList, data, width, height);
        });

    auto gtao_denoise_pass = pRenderGraph->AddPass<GTAODenoisePassData>("GTAO denoise",
        [&](GTAODenoisePassData& data, RenderGraphBuilder& builder)
        {
            data.inputAOTerm = builder.Read(gtao_pass->outputAOTerm, GfxResourceState::ShaderResourceNonPS);
            data.inputEdge = builder.Read(gtao_pass->outputEdge, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = m_bEnableGTSO ? GfxFormat::R32UI : GfxFormat::R8UI;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputAOTerm = builder.Create<RenderGraphTexture>(desc, "GTAO denoised AO");
            data.outputAOTerm = builder.Write(data.outputAOTerm, GfxResourceState::UnorderedAccess);
        },
        [=](const GTAODenoisePassData& data, IGfxCommandList* pCommandList)
        {
            Denoise(pCommandList, data, width, height);
        });

    return gtao_denoise_pass->outputAOTerm;
}

void GTAO::FilterDepth(IGfxCommandList* pCommandList, const GTAOFilterDepthPassData& data, uint32_t width, uint32_t height)
{
    UpdateGTAOConstants(pCommandList, width, height);

    pCommandList->SetPipelineState(m_pPrefilterDepthPSO);

    RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();
    RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputDepth);
    RenderGraphTexture* hzbRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputDepthMip0);

    struct CB
    {
        uint srcRawDepth;
        uint outWorkingDepthMIP0;
        uint outWorkingDepthMIP1;
        uint outWorkingDepthMIP2;

        uint outWorkingDepthMIP3;
        uint outWorkingDepthMIP4;
        uint2 _padding;
    };

    CB cb;
    cb.srcRawDepth = depthRT->GetSRV()->GetHeapIndex();
    cb.outWorkingDepthMIP0 = hzbRT->GetUAV(0, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP1 = hzbRT->GetUAV(1, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP2 = hzbRT->GetUAV(2, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP3 = hzbRT->GetUAV(3, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP4 = hzbRT->GetUAV(4, 0)->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    // note: in gtao_prefilter_depth_16x16 each is thread group handles a 16x16 block (with [numthreads(8, 8, 1)] and each logical thread handling a 2x2 block)
    pCommandList->Dispatch((width + 15) / 16, (height + 15) / 16, 1);
}

void GTAO::Draw(IGfxCommandList* pCommandList, const GTAOPassData& data, uint32_t width, uint32_t height)
{
    switch (m_qualityLevel)
    {
    case 0:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOLowPSO : m_pGTAOLowPSO);
        break;
    case 1:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOMediumPSO : m_pGTAOMediumPSO);
        break;
    case 2:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOHighPSO : m_pGTAOHighPSO);
        break;
    case 3:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOUltraPSO : m_pGTAOUltraPSO);
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();
    RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputFilteredDepth);
    RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputNormal);
    RenderGraphTexture* aoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputAOTerm);
    RenderGraphTexture* edgeRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputEdge);

    struct CB
    {
        uint srcWorkingDepth;
        uint normalRT;
        uint hilbertLUT;
        uint outWorkingAOTerm;
        uint outWorkingEdges;
    };

    CB cb;
    cb.srcWorkingDepth = depthRT->GetSRV()->GetHeapIndex();
    cb.normalRT = normalRT->GetSRV()->GetHeapIndex();
    cb.hilbertLUT = m_pHilbertLUT->GetSRV()->GetHeapIndex();
    cb.outWorkingAOTerm = aoRT->GetUAV()->GetHeapIndex();
    cb.outWorkingEdges = edgeRT->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    pCommandList->Dispatch((width + XE_GTAO_NUMTHREADS_X - 1)/ XE_GTAO_NUMTHREADS_X, (height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
}

void GTAO::Denoise(IGfxCommandList* pCommandList, const GTAODenoisePassData& data, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_bEnableGTSO ? m_pSODenoisePSO : m_pDenoisePSO);

    RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();
    RenderGraphTexture* inputAO = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputAOTerm);
    RenderGraphTexture* inputEdge = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputEdge);
    RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputAOTerm);

    uint32_t cb[3] =
    {
        inputAO->GetSRV()->GetHeapIndex(),
        inputEdge->GetSRV()->GetHeapIndex(),
        outputRT->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch((width + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2), (height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
}

void GTAO::CreateHilbertLUT()
{
    uint16_t data[64 * 64];

    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            uint32_t r2index = XeGTAO::HilbertIndex(x, y);
            RE_ASSERT(r2index < 65536);
            data[x + 64 * y] = (uint16_t)r2index;
        }
    }

    m_pHilbertLUT.reset(m_pRenderer->CreateTexture2D(64, 64, 1, GfxFormat::R16UI, 0, "GTAO::m_pHilbertLUT"));
    m_pRenderer->UploadTexture(m_pHilbertLUT->GetTexture(), data);
}

void GTAO::UpdateGTAOConstants(IGfxCommandList* pCommandList, uint32_t width, uint32_t height)
{
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    float4x4 projection = camera->GetProjectionMatrix();

    XeGTAO::GTAOSettings settings;
    settings.QualityLevel = m_qualityLevel;
    settings.Radius = m_radius;

    XeGTAO::GTAOConstants consts;
    XeGTAO::GTAOUpdateConstants(consts, width, height, settings, &projection.x.x, true, m_pRenderer->GetFrameID() % 256);

    pCommandList->SetComputeConstants(2, &consts, sizeof(consts));
}
