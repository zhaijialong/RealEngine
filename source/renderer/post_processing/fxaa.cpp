#include "fxaa.h"
#include "../renderer.h"
#include "utils/gui_util.h"

FXAA::FXAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("fxaa.hlsl", "cs_main", GfxShaderType::CS);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "FXAA PSO");
}

void FXAA::OnGui()
{
    if (ImGui::CollapsingHeader("FXAA"))
    {
        ImGui::Checkbox("Enable##FXAA", &m_bEnable);
    }
}

RGHandle FXAA::AddPass(RenderGraph* pRenderGraph, RGHandle inputHandle, uint32_t width, uint32_t height)
{
    if (!m_bEnable)
    {
        return inputHandle;
    }

    struct FXAAData
    {
        RGHandle input;
        RGHandle output;
    };

    auto fxaa_pass = pRenderGraph->AddPass<FXAAData>("FXAA", RenderPassType::Compute,
        [&](FXAAData& data, RGBuilder& builder)
        {
            data.input = builder.Read(inputHandle);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            data.output = builder.Create<RGTexture>(desc, "FXAA Output");
            data.output = builder.Write(data.output);
        },
        [=](const FXAAData& data, IGfxCommandList* pCommandList)
        {
            Render(pCommandList, 
                pRenderGraph->GetTexture(data.input), 
                pRenderGraph->GetTexture(data.output), 
                width, height);
        });

    return fxaa_pass->output;
}

void FXAA::Render(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint c_ldrTexture;
        uint c_outTexture;
        uint c_linearSampler;
        uint c_width;
        uint c_height;
        float c_rcpWidth;
        float c_rcpHeight;
    };

    CB constantBuffer = {
        input->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(),
        m_pRenderer->GetLinearSampler()->GetHeapIndex(),
        width,
        height,
        1.0f / width,
        1.0f / height,
    };

    pCommandList->SetComputeConstants(1, &constantBuffer, sizeof(constantBuffer));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
