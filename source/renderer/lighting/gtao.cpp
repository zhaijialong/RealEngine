#include "gtao.h"
#include "../renderer.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "utils/gui_util.h"
#include "XeGTAO.h"

GTAO::GTAO(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    eastl::vector<eastl::string> defines;
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

RGHandle GTAO::AddPass(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, uint32_t width, uint32_t height)
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
        return RGHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "GTAO");

    struct FilterDepthPassData
    {
        RGHandle inputDepth;
        RGHandle outputDepthMip0;
        RGHandle outputDepthMip1;
        RGHandle outputDepthMip2;
        RGHandle outputDepthMip3;
        RGHandle outputDepthMip4;
    };

    auto gtao_filter_depth_pass = pRenderGraph->AddPass<FilterDepthPassData>("GTAO filter depth", RenderPassType::Compute,
        [&](FilterDepthPassData& data, RGBuilder& builder)
        {
            data.inputDepth = builder.Read(depthRT);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.mip_levels = 5;
            desc.format = GfxFormat::R16F;
            RGHandle gtao_depth = builder.Create<RGTexture>(desc, "GTAO depth(HZB)");

            data.outputDepthMip0 = builder.Write(gtao_depth, 0);
            data.outputDepthMip1 = builder.Write(gtao_depth, 1);
            data.outputDepthMip2 = builder.Write(gtao_depth, 2);
            data.outputDepthMip3 = builder.Write(gtao_depth, 3);
            data.outputDepthMip4 = builder.Write(gtao_depth, 4);
        },
        [=](const FilterDepthPassData& data, IGfxCommandList* pCommandList)
        {
            FilterDepth(pCommandList, 
                pRenderGraph->GetTexture(data.inputDepth),
                pRenderGraph->GetTexture(data.outputDepthMip0),
                width, height);
        });

    struct GTAOPassData
    {
        RGHandle inputFilteredDepth;
        RGHandle inputNormal;
        RGHandle outputAOTerm;
        RGHandle outputEdge;
    };

    auto gtao_pass = pRenderGraph->AddPass<GTAOPassData>("GTAO", RenderPassType::Compute,
        [&](GTAOPassData& data, RGBuilder& builder)
        {
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip0, 0);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip1, 1);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip2, 2);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip3, 3);
            data.inputFilteredDepth = builder.Read(gtao_filter_depth_pass->outputDepthMip4, 4);

            data.inputNormal = builder.Read(normalRT);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = m_bEnableGTSO ? GfxFormat::R32UI : GfxFormat::R8UI;
            data.outputAOTerm = builder.Create<RGTexture>(desc, "GTAO AOTerm");
            data.outputAOTerm = builder.Write(data.outputAOTerm);

            desc.format = GfxFormat::R8UNORM;
            data.outputEdge = builder.Create<RGTexture>(desc, "GTAO Edge");
            data.outputEdge = builder.Write(data.outputEdge);
        },
        [=](const GTAOPassData& data, IGfxCommandList* pCommandList)
        {
            RenderAO(pCommandList,
                pRenderGraph->GetTexture(data.inputFilteredDepth),
                pRenderGraph->GetTexture(data.inputNormal),
                pRenderGraph->GetTexture(data.outputAOTerm),
                pRenderGraph->GetTexture(data.outputEdge),
                width, height);
        });

    struct DenoisePassData
    {
        RGHandle inputAOTerm;
        RGHandle inputEdge;
        RGHandle outputAOTerm;
    };

    auto gtao_denoise_pass = pRenderGraph->AddPass<DenoisePassData>("GTAO denoise", RenderPassType::Compute,
        [&](DenoisePassData& data, RGBuilder& builder)
        {
            data.inputAOTerm = builder.Read(gtao_pass->outputAOTerm);
            data.inputEdge = builder.Read(gtao_pass->outputEdge);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = m_bEnableGTSO ? GfxFormat::R32UI : GfxFormat::R8UI;
            data.outputAOTerm = builder.Create<RGTexture>(desc, "GTAO denoised AO");
            data.outputAOTerm = builder.Write(data.outputAOTerm);
        },
        [=](const DenoisePassData& data, IGfxCommandList* pCommandList)
        {
            Denoise(pCommandList, 
                pRenderGraph->GetTexture(data.inputAOTerm),
                pRenderGraph->GetTexture(data.inputEdge),
                pRenderGraph->GetTexture(data.outputAOTerm),
                width, height);
        });

    return gtao_denoise_pass->outputAOTerm;
}

void GTAO::FilterDepth(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* hzb, uint32_t width, uint32_t height)
{
    UpdateGTAOConstants(pCommandList, width, height);

    pCommandList->SetPipelineState(m_pPrefilterDepthPSO);

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
    cb.srcRawDepth = depth->GetSRV()->GetHeapIndex();
    cb.outWorkingDepthMIP0 = hzb->GetUAV(0, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP1 = hzb->GetUAV(1, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP2 = hzb->GetUAV(2, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP3 = hzb->GetUAV(3, 0)->GetHeapIndex();
    cb.outWorkingDepthMIP4 = hzb->GetUAV(4, 0)->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    // note: in gtao_prefilter_depth_16x16 each is thread group handles a 16x16 block (with [numthreads(8, 8, 1)] and each logical thread handling a 2x2 block)
    pCommandList->Dispatch(DivideRoudingUp(width, 16), DivideRoudingUp(height, 16), 1);
}

void GTAO::RenderAO(IGfxCommandList* pCommandList, RGTexture* hzb, RGTexture* normal, RGTexture* outputAO, RGTexture* outputEdge, uint32_t width, uint32_t height)
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

    struct CB
    {
        uint srcWorkingDepth;
        uint normalRT;
        uint hilbertLUT;
        uint outWorkingAOTerm;
        uint outWorkingEdges;
    };

    CB cb;
    cb.srcWorkingDepth = hzb->GetSRV()->GetHeapIndex();
    cb.normalRT = normal->GetSRV()->GetHeapIndex();
    cb.hilbertLUT = m_pHilbertLUT->GetSRV()->GetHeapIndex();
    cb.outWorkingAOTerm = outputAO->GetUAV()->GetHeapIndex();
    cb.outWorkingEdges = outputEdge->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, XE_GTAO_NUMTHREADS_X), DivideRoudingUp(height, XE_GTAO_NUMTHREADS_Y), 1);
}

void GTAO::Denoise(IGfxCommandList* pCommandList, RGTexture* inputAO, RGTexture* edge, RGTexture* outputAO, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_bEnableGTSO ? m_pSODenoisePSO : m_pDenoisePSO);

    uint32_t cb[3] =
    {
        inputAO->GetSRV()->GetHeapIndex(),
        edge->GetSRV()->GetHeapIndex(),
        outputAO->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, XE_GTAO_NUMTHREADS_X * 2), DivideRoudingUp(height, XE_GTAO_NUMTHREADS_Y), 1);
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

    pCommandList->SetComputeConstants(1, &consts, sizeof(consts));
}
