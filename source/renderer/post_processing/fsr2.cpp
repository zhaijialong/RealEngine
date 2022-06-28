#include "fsr2.h"
#include "../renderer.h"
#include "utils/gui_util.h"

FSR2::FSR2(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle FSR2::Render(RenderGraph* pRenderGraph, RenderGraphHandle input, uint32_t renderWidth, uint32_t renderHeight)
{
    GUI("PostProcess", "FSR 2.0", [&]()
        {

        });

    struct FSR2Data
    {
        RenderGraphHandle input;
        RenderGraphHandle output;
    };

    auto fsr2_pass = pRenderGraph->AddPass<FSR2Data>("FSR 2.0", RenderPassType::Compute,
        [&](FSR2Data& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(input);

            RenderGraphTexture::Desc desc;
            desc.width = m_pRenderer->GetDisplayWidth();
            desc.height = m_pRenderer->GetDisplayHeight();
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "FSR2 Output");
            data.output = builder.Write(data.output);
        },
        [=](const FSR2Data& data, IGfxCommandList* pCommandList)
        {
            GfxComputePipelineDesc psoDesc;
            psoDesc.cs = m_pRenderer->GetShader("fsr2_tmp.hlsl", "main", "cs_6_6", {});
            IGfxPipelineState* pso = m_pRenderer->GetPipelineState(psoDesc, "FSR2 tmp PSO");

            pCommandList->SetPipelineState(pso);

            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            uint consts[2] = { input->GetSRV()->GetHeapIndex(), output->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, consts, sizeof(consts));

            pCommandList->Dispatch((m_pRenderer->GetDisplayWidth() + 7) / 8, (m_pRenderer->GetDisplayHeight() + 7) / 8, 1);
        });

    return fsr2_pass->output;
}
