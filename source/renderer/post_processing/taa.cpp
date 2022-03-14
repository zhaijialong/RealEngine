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

RenderGraphHandle TAA::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, RenderGraphHandle sceneDepthRT,
    RenderGraphHandle linearDepthRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "TAA",
        [&]()
        {
            if (ImGui::Checkbox("Enable##TAA", &m_bEnable))
            {
                Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
                camera->EnableJitter(m_bEnable);

                if (!m_bEnable)
                {
                    m_pHistoryColorInput.reset();
                    m_pHistoryColorOutput.reset();
                }

                m_bHistoryInvalid = true;
            }
        });

    if (!m_bEnable)
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
        RenderGraphHandle prevLinearDepthRT;

        RenderGraphHandle outputRT;
        RenderGraphHandle historyOutputRT;
    };

    auto taa_pass = pRenderGraph->AddPass<TAAPassData>("TAA",
        [&](TAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphHandle historyInputRT = builder.Import(m_pHistoryColorInput->GetTexture(), GfxResourceState::UnorderedAccess);
            RenderGraphHandle historyOutputRT = builder.Import(m_pHistoryColorOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);

            data.inputRT = builder.Read(sceneColorRT, GfxResourceState::ShaderResourceNonPS);
            data.historyInputRT = builder.Read(historyInputRT, GfxResourceState::ShaderResourceNonPS);
            data.velocityRT = builder.Read(velocityRT, GfxResourceState::ShaderResourceNonPS);
            data.linearDepthRT = builder.Read(linearDepthRT, GfxResourceState::ShaderResourceNonPS);
            data.prevLinearDepthRT = builder.Read(m_pRenderer->GetPrevLinearDepthHandle(), GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Output");

            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);
            data.historyOutputRT = builder.Write(historyOutputRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocityRT);
            RenderGraphTexture* linearDepthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.linearDepthRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRT);

            Draw(pCommandList, inputRT->GetSRV(), velocityRT->GetSRV(), linearDepthRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    return taa_pass->outputRT;
}

void TAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* velocity, IGfxDescriptor* linearDepth, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    IGfxDescriptor* historyInput = m_pHistoryColorInput->GetSRV();
    if (m_bHistoryInvalid)
    {
        historyInput = input;
        m_bHistoryInvalid = false;
    }

    struct CB
    {
        uint inputRT;
        uint historyInputRT;
        uint velocityRT;
        uint linearDepthRT;
        uint prevLinearDepthRT;

        uint historyOutputRT;
        uint outputRT;
    };

    CB cb;
    cb.inputRT = input->GetHeapIndex();
    cb.historyInputRT = historyInput->GetHeapIndex();
    cb.velocityRT = velocity->GetHeapIndex();
    cb.linearDepthRT = linearDepth->GetHeapIndex();
    cb.prevLinearDepthRT = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    cb.historyOutputRT = m_pHistoryColorOutput->GetUAV()->GetHeapIndex();
    cb.outputRT = output->GetHeapIndex();
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    eastl::swap(m_pHistoryColorInput, m_pHistoryColorOutput);
}
