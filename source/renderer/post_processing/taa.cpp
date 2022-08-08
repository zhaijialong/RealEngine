#include "taa.h"
#include "../renderer.h"
#include "utils/gui_util.h"

TAA::TAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "TAA PSO");
}

RenderGraphHandle TAA::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, RenderGraphHandle linearDepthRT,
    RenderGraphHandle velocityRT, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "TAA",
        [&]()
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
        });

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
        RenderGraphHandle inputRT;
        RenderGraphHandle historyInputRT;
        RenderGraphHandle velocityRT;
        RenderGraphHandle linearDepthRT;

        RenderGraphHandle outputRT;
        RenderGraphHandle historyOutputRT;
    };

    auto taa_pass = pRenderGraph->AddPass<TAAPassData>("TAA", RenderPassType::Compute,
        [&](TAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphHandle historyInputRT = builder.Import(m_pHistoryColorInput->GetTexture(), GfxResourceState::UnorderedAccess);
            RenderGraphHandle historyOutputRT = builder.Import(m_pHistoryColorOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);

            data.inputRT = builder.Read(sceneColorRT);
            data.historyInputRT = builder.Read(historyInputRT);
            data.velocityRT = builder.Read(velocityRT);
            data.linearDepthRT = builder.Read(linearDepthRT);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Output");

            data.outputRT = builder.Write(data.outputRT);
            data.historyOutputRT = builder.Write(historyOutputRT);
        },
        [=](const TAAPassData& data, IGfxCommandList* pCommandList)
        {
            Draw(pCommandList,
                pRenderGraph->GetTexture(data.inputRT), 
                pRenderGraph->GetTexture(data.velocityRT), 
                pRenderGraph->GetTexture(data.linearDepthRT),
                pRenderGraph->GetTexture(data.outputRT), width, height);
        });

    return taa_pass->outputRT;
}

void TAA::Draw(IGfxCommandList* pCommandList, RenderGraphTexture* input, RenderGraphTexture* velocity, RenderGraphTexture* linearDepth, RenderGraphTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint inputRT;
        uint historyInputRT;
        uint velocityRT;
        uint linearDepthRT;

        uint historyOutputRT;
        uint outputRT;
    };

    CB cb;
    cb.inputRT = input->GetSRV()->GetHeapIndex();
    cb.velocityRT = velocity->GetSRV()->GetHeapIndex();
    cb.linearDepthRT = linearDepth->GetSRV()->GetHeapIndex();
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

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    eastl::swap(m_pHistoryColorInput, m_pHistoryColorOutput);
}
