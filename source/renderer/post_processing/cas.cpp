#include "cas.h"
#include "../renderer.h"
#include "utils/gui_util.h"

// CAS
#define A_CPU
#include "ffx_a.h"
#include "ffx_cas.h"

CAS::CAS(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("cas.hlsl", "main", "cs_6_6", {"CAS_FP16=0"});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "CAS PSO");
}

RenderGraphHandle CAS::Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "CAS",
        [&]()
        {
            ImGui::Checkbox("Enable##CAS", &m_bEnable);
            ImGui::SliderFloat("Sharpness##CAS", &m_sharpenVal, 0.0f, 1.0f, "%.2f");
        });

    if (!m_bEnable)
    {
        return inputHandle;
    }

    struct CASPassData
    {
        RenderGraphHandle inRT;
        RenderGraphHandle outRT;
    };

    auto cas_pass = pRenderGraph->AddPass<CASPassData>("CAS", RenderPassType::Compute,
        [&](CASPassData& data, RenderGraphBuilder& builder)
        {
            data.inRT = builder.Read(inputHandle);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            data.outRT = builder.Create<RenderGraphTexture>(desc, "CAS Output");
            data.outRT = builder.Write(data.outRT);
        },
        [=](const CASPassData& data, IGfxCommandList* pCommandList)
        {
            Draw(pCommandList,
                pRenderGraph->GetTexture(data.inRT), 
                pRenderGraph->GetTexture(data.outRT), 
                width, height);
        });

    return cas_pass->outRT;
}

void CAS::Draw(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    struct CASConstants
    {
        uint input;
        uint output;
        uint2 __padding;

        uint4 const0;
        uint4 const1;
    };

    CASConstants consts;
    consts.input = input->GetSRV()->GetHeapIndex();
    consts.output = output->GetUAV()->GetHeapIndex();

    CasSetup(reinterpret_cast<AU1*>(&consts.const0), reinterpret_cast<AU1*>(&consts.const1), m_sharpenVal, (AF1)width, (AF1)height, (AF1)width, (AF1)height);

    pCommandList->SetComputeConstants(1, &consts, sizeof(consts));

    // This value is the image region dim that each thread group of the CAS shader operates on
    static const int threadGroupWorkRegionDim = 16;
    int dispatchX = (width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
    int dispatchY = (height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

    pCommandList->Dispatch(dispatchX, dispatchY, 1);
}
