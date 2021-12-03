#include "gtao.h"
#include "../renderer.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "XeGTAO.h"

GTAO::GTAO(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    std::vector<std::string> defines;
    defines.push_back("XE_GTAO_USE_HALF_FLOAT_PRECISION=0");
    defines.push_back("XE_GTAO_COMPUTE_BENT_NORMALS=1");

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_prefilter_depth_16x16", "cs_6_6", defines);
    m_pPrefilterDepthPSO = pRenderer->GetPipelineState(psoDesc, "GTAO prefilter depth PSO");

    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_main", "cs_6_6", defines);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "GTAO PSO");

    psoDesc.cs = pRenderer->GetShader("gtao.hlsl", "gtao_denoise", "cs_6_6", defines);
    m_pDenoisePSO = pRenderer->GetPipelineState(psoDesc, "GTAO denoise PSO");

    CreateHilbertLUT();
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
    pCommandList->SetPipelineState(m_pPSO);

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
    pCommandList->SetPipelineState(m_pDenoisePSO);

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

    m_pHilbertLUT.reset(m_pRenderer->CreateTexture2D(64, 64, 1, GfxFormat::R16UI, GfxTextureUsageShaderResource, "GTAO::m_pHilbertLUT"));
    m_pRenderer->UploadTexture(m_pHilbertLUT->GetTexture(), data);
}

void GTAO::UpdateGTAOConstants(IGfxCommandList* pCommandList, uint32_t width, uint32_t height)
{
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    float4x4 projection = camera->GetProjectionMatrix();

    XeGTAO::GTAOSettings settings;

    XeGTAO::GTAOConstants consts;
    XeGTAO::GTAOUpdateConstants(consts, width, height, settings, &projection.x.x, true, m_pRenderer->GetFrameID() % 256);

    pCommandList->SetComputeConstants(2, &consts, sizeof(consts));
}
