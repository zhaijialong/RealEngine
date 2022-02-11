#include "fxaa.h"
#include "../renderer.h"

FXAA::FXAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("fxaa.hlsl", "cs_main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "FXAA PSO");
}

RenderGraphHandle FXAA::Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, uint32_t width, uint32_t height)
{
    struct FXAAData
    {
        RenderGraphHandle input;
        RenderGraphHandle output;
    };

    auto fxaa_pass = pRenderGraph->AddPass<FXAAData>("FXAA",
        [&](FXAAData& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(inputHandle, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.output = builder.Create<RenderGraphTexture>(desc, "FXAA Output");
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const FXAAData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);

            Draw(pCommandList, input->GetSRV(), output->GetUAV(), width, height);
        });

    return fxaa_pass->output;
}

void FXAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height)
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
        input->GetHeapIndex(), 
        output->GetHeapIndex(),
        m_pRenderer->GetLinearSampler()->GetHeapIndex(),
        width,
        height,
        1.0f / width,
        1.0f / height,
    };

    pCommandList->SetComputeConstants(1, &constantBuffer, sizeof(constantBuffer));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
