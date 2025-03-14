#include "taa.h"
#include "../renderer.h"
#include "utils/gui_util.h"

TAA::TAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "main", GfxShaderType::CS);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "TAA PSO");
}

void TAA::OnGui()
{
    if (ImGui::CollapsingHeader("TAA"))
    {
        if (ImGui::Checkbox("Enable##TAA", &m_bEnable))
        {
            if (!m_bEnable)
            {
                m_pHistoryColorInput.reset();
                m_pHistoryColorOutput.reset();
            }

            m_bHistoryInvalid = true;
        }
    }
}

RGHandle TAA::AddPass(RenderGraph* pRenderGraph, RGHandle sceneColorRT, RGHandle sceneDepthRT, RGHandle velocityRT, uint32_t width, uint32_t height)
{
    if (!m_bEnable || m_pRenderer->GetOutputType() == RendererOutput::PathTracing)
    {
        return sceneColorRT;
    }

    if (m_pHistoryColorInput == nullptr ||
        m_pHistoryColorInput->GetTexture()->GetDesc().width != width ||
        m_pHistoryColorInput->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryColorInput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "TAA HistoryTexture 0"));
        m_pHistoryColorOutput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "TAA HistoryTexture 1"));
        m_bHistoryInvalid = true;
    }

    struct TAAPassData
    {
        RGHandle inputRT;
        RGHandle historyInputRT;
        RGHandle velocityRT;
        RGHandle depthRT;

        RGHandle outputRT;
        RGHandle historyOutputRT;
    };

    auto taa_pass = pRenderGraph->AddPass<TAAPassData>("TAA", RenderPassType::Compute,
        [&](TAAPassData& data, RGBuilder& builder)
        {
            RGHandle historyInputRT = builder.Import(m_pHistoryColorInput->GetTexture(), GfxAccessComputeUAV);
            RGHandle historyOutputRT = builder.Import(m_pHistoryColorOutput->GetTexture(), m_bHistoryInvalid ? GfxAccessComputeUAV : GfxAccessComputeSRV);

            data.inputRT = builder.Read(sceneColorRT);
            data.historyInputRT = builder.Read(historyInputRT);
            data.velocityRT = builder.Read(velocityRT);
            data.depthRT = builder.Read(sceneDepthRT);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputRT = builder.Create<RGTexture>(desc, "TAA Output");

            data.outputRT = builder.Write(data.outputRT);
            data.historyOutputRT = builder.Write(historyOutputRT);
        },
        [=](const TAAPassData& data, IGfxCommandList* pCommandList)
        {
            Render(pCommandList,
                pRenderGraph->GetTexture(data.inputRT), 
                pRenderGraph->GetTexture(data.velocityRT), 
                pRenderGraph->GetTexture(data.depthRT),
                pRenderGraph->GetTexture(data.outputRT), width, height);
        });

    return taa_pass->outputRT;
}

void TAA::Render(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* velocity, RGTexture* depth, RGTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint inputRT;
        uint historyInputRT;
        uint velocityRT;
        uint depthRT;

        uint historyOutputRT;
        uint outputRT;
    };

    CB cb;
    cb.inputRT = input->GetSRV()->GetHeapIndex();
    cb.velocityRT = velocity->GetSRV()->GetHeapIndex();
    cb.depthRT = depth->GetSRV()->GetHeapIndex();
    if (m_bHistoryInvalid)
    {
        cb.historyInputRT = input->GetSRV()->GetHeapIndex();
        m_bHistoryInvalid = false;
    }
    else
    {
        cb.historyInputRT = m_pHistoryColorInput->GetSRV()->GetHeapIndex();
    }
    cb.historyOutputRT = m_pHistoryColorOutput->GetUAV()->GetHeapIndex();
    cb.outputRT = output->GetUAV()->GetHeapIndex();
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);

    eastl::swap(m_pHistoryColorInput, m_pHistoryColorOutput);
}
