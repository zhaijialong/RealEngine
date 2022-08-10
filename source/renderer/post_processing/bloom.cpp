#include "bloom.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "fmt/format.h"

Bloom::Bloom(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("bloom.hlsl", "downsampling", "cs_6_6", {});
    m_pDownsamplePSO = pRenderer->GetPipelineState(desc, "Bloom downsample PSO");

    desc.cs = pRenderer->GetShader("bloom.hlsl", "downsampling", "cs_6_6", { "FIRST_PASS=1" });
    m_pFirstDownsamplePSO = pRenderer->GetPipelineState(desc, "Bloom downsample PSO");

    desc.cs = pRenderer->GetShader("bloom.hlsl", "upsampling", "cs_6_6", {});
    m_pUpsamplePSO = pRenderer->GetPipelineState(desc, "Bloom upsample PSO");
}

RGHandle Bloom::Render(RenderGraph* pRenderGraph, RGHandle sceneColorRT, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "Bloom",
        [&]()
        {
            ImGui::Checkbox("Enable##Bloom", &m_bEnable);
            ImGui::SliderFloat("Threshold##Bloom", &m_threshold, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Intensity##Bloom", &m_intensity, 0.0f, 10.0f, "%.2f");
        });

    if (!m_bEnable)
    {
        return RGHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "Bloom");

    for (uint i = 0; i < 7; ++i)
    {
        m_mipSize[i] = uint2(width, height);

        width = eastl::max(1u, (width + 1) >> 1);
        height = eastl::max(1u, (height + 1) >> 1);
    }

    RGHandle downsampleMip1 = DownsamplePass(pRenderGraph, sceneColorRT, 1);
    RGHandle downsampleMip2 = DownsamplePass(pRenderGraph, downsampleMip1, 2);
    RGHandle downsampleMip3 = DownsamplePass(pRenderGraph, downsampleMip2, 3);
    RGHandle downsampleMip4 = DownsamplePass(pRenderGraph, downsampleMip3, 4);
    RGHandle downsampleMip5 = DownsamplePass(pRenderGraph, downsampleMip4, 5);
    RGHandle downsampleMip6 = DownsamplePass(pRenderGraph, downsampleMip5, 6);

    RGHandle upsampleMip6 = downsampleMip6;
    RGHandle upsampleMip5 = UpsamplePass(pRenderGraph, downsampleMip5, upsampleMip6, 5);
    RGHandle upsampleMip4 = UpsamplePass(pRenderGraph, downsampleMip4, upsampleMip5, 4);
    RGHandle upsampleMip3 = UpsamplePass(pRenderGraph, downsampleMip3, upsampleMip4, 3);
    RGHandle upsampleMip2 = UpsamplePass(pRenderGraph, downsampleMip2, upsampleMip3, 2);
    RGHandle upsampleMip1 = UpsamplePass(pRenderGraph, downsampleMip1, upsampleMip2, 1);

    return upsampleMip1;
}

RGHandle Bloom::DownsamplePass(RenderGraph* pRenderGraph, RGHandle input, uint32_t mip)
{
    struct DownsamplePassData
    {
        RGHandle input;
        RGHandle output;
    };

    eastl::string name = fmt::format("Bloom DownsampleMip{}({}x{})", mip, m_mipSize[mip].x, m_mipSize[mip].y).c_str();

    auto downsample_pass = pRenderGraph->AddPass<DownsamplePassData>(name, RenderPassType::Compute,
        [&](DownsamplePassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);

            RGTexture::Desc desc;
            desc.width = m_mipSize[mip].x;
            desc.height = m_mipSize[mip].y;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Create<RGTexture>(desc, name);
            data.output = builder.Write(data.output);
        },
        [=](const DownsamplePassData& data, IGfxCommandList* pCommandList)
        {
            Downsample(pCommandList, 
                pRenderGraph->GetTexture(data.input), 
                pRenderGraph->GetTexture(data.output), 
                mip);
        });

    return downsample_pass->output;
}

RGHandle Bloom::UpsamplePass(RenderGraph* pRenderGraph, RGHandle highInput, RGHandle lowInput, uint32_t mip)
{
    struct UpsamplePassData
    {
        RGHandle lowInput;
        RGHandle highInput;
        RGHandle output;
    };

    eastl::string name = fmt::format("Bloom UpsampleMip{}({}x{})", mip, m_mipSize[mip].x, m_mipSize[mip].y).c_str();

    auto upsample_pass = pRenderGraph->AddPass<UpsamplePassData>(name, RenderPassType::Compute,
        [&](UpsamplePassData& data, RGBuilder& builder)
        {
            data.lowInput = builder.Read(lowInput);
            data.highInput = builder.Read(highInput);

            RGTexture::Desc desc;
            desc.width = m_mipSize[mip].x;
            desc.height = m_mipSize[mip].y;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Create<RGTexture>(desc, name);
            data.output = builder.Write(data.output);
        },
        [=](const UpsamplePassData& data, IGfxCommandList* pCommandList)
        {
            Upsample(pCommandList, 
                pRenderGraph->GetTexture(data.highInput), 
                pRenderGraph->GetTexture(data.lowInput), 
                pRenderGraph->GetTexture(data.output), 
                mip);
        });

    return upsample_pass->output;
}

void Bloom::Downsample(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t mip)
{
    pCommandList->SetPipelineState(mip == 1 ? m_pFirstDownsamplePSO : m_pDownsamplePSO);

    struct DownsampleConstants
    {
        float2 inputPixelSize; //high mip
        float2 outputPixelSize; //low mip
        uint inputTexture;
        uint outputTexture;
        float threshold;
    };

    uint32_t input_width = m_mipSize[mip - 1].x;
    uint32_t input_height = m_mipSize[mip - 1].y;
    uint32_t output_width = m_mipSize[mip].x;
    uint32_t output_height = m_mipSize[mip].y;

    DownsampleConstants constants;
    constants.inputTexture = input->GetSRV()->GetHeapIndex();
    constants.outputTexture = output->GetUAV()->GetHeapIndex();
    constants.inputPixelSize = float2(1.0f / input_width, 1.0f / input_height);
    constants.outputPixelSize = float2(1.0f / output_width, 1.0f / output_height);
    constants.threshold = m_threshold;

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((output_width + 7) / 8, (output_height + 7) / 8, 1);
}

void Bloom::Upsample(IGfxCommandList* pCommandList, RGTexture* highInput, RGTexture* lowInput, RGTexture* output, uint32_t mip)
{
    pCommandList->SetPipelineState(m_pUpsamplePSO);

    struct UpsampleConstants
    {
        float2 inputPixelSize; //low mip
        float2 outputPixelSize; //high mip
        uint inputLowTexture;
        uint inputHighTexture;
        uint outputTexture;
    };

    uint32_t input_width = m_mipSize[mip + 1].x;
    uint32_t input_height = m_mipSize[mip + 1].y;
    uint32_t output_width = m_mipSize[mip].x;
    uint32_t output_height = m_mipSize[mip].y;

    UpsampleConstants constants;
    constants.inputLowTexture = lowInput->GetSRV()->GetHeapIndex();
    constants.inputHighTexture = highInput->GetSRV()->GetHeapIndex();
    constants.outputTexture = output->GetUAV()->GetHeapIndex();
    constants.inputPixelSize = float2(1.0f / input_width, 1.0f / input_height);
    constants.outputPixelSize = float2(1.0f / output_width, 1.0f / output_height);

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((output_width + 7) / 8, (output_height + 7) / 8, 1);
}
