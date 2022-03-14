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

RenderGraphHandle Bloom::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, uint32_t width, uint32_t height)
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
        return RenderGraphHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "Bloom");

    for (uint i = 0; i < 7; ++i)
    {
        m_mipSize[i] = uint2(width, height);

        width = eastl::max(1u, (width + 1) >> 1);
        height = eastl::max(1u, (height + 1) >> 1);
    }

    RenderGraphHandle downsampleMip1 = DownsamplePass(pRenderGraph, sceneColorRT, 1);
    RenderGraphHandle downsampleMip2 = DownsamplePass(pRenderGraph, downsampleMip1, 2);
    RenderGraphHandle downsampleMip3 = DownsamplePass(pRenderGraph, downsampleMip2, 3);
    RenderGraphHandle downsampleMip4 = DownsamplePass(pRenderGraph, downsampleMip3, 4);
    RenderGraphHandle downsampleMip5 = DownsamplePass(pRenderGraph, downsampleMip4, 5);
    RenderGraphHandle downsampleMip6 = DownsamplePass(pRenderGraph, downsampleMip5, 6);

    RenderGraphHandle upsampleMip6 = downsampleMip6;
    RenderGraphHandle upsampleMip5 = UpsamplePass(pRenderGraph, downsampleMip5, upsampleMip6, 5);
    RenderGraphHandle upsampleMip4 = UpsamplePass(pRenderGraph, downsampleMip4, upsampleMip5, 4);
    RenderGraphHandle upsampleMip3 = UpsamplePass(pRenderGraph, downsampleMip3, upsampleMip4, 3);
    RenderGraphHandle upsampleMip2 = UpsamplePass(pRenderGraph, downsampleMip2, upsampleMip3, 2);
    RenderGraphHandle upsampleMip1 = UpsamplePass(pRenderGraph, downsampleMip1, upsampleMip2, 1);

    return upsampleMip1;
}

RenderGraphHandle Bloom::DownsamplePass(RenderGraph* pRenderGraph, RenderGraphHandle input, uint32_t mip)
{
    struct DownsamplePassData
    {
        RenderGraphHandle input;
        RenderGraphHandle output;
    };

    eastl::string name = fmt::format("Bloom DownsampleMip{}({}x{})", mip, m_mipSize[mip].x, m_mipSize[mip].y).c_str();

    auto downsample_pass = pRenderGraph->AddPass<DownsamplePassData>(name,
        [&](DownsamplePassData& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(input, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_mipSize[mip].x;
            desc.height = m_mipSize[mip].y;
            desc.format = GfxFormat::R11G11B10F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.output = builder.Create<RenderGraphTexture>(desc, name);
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const DownsamplePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Downsample(pCommandList, input->GetSRV(), output->GetUAV(), mip);
        });

    return downsample_pass->output;
}

RenderGraphHandle Bloom::UpsamplePass(RenderGraph* pRenderGraph, RenderGraphHandle highInput, RenderGraphHandle lowInput, uint32_t mip)
{
    struct UpsamplePassData
    {
        RenderGraphHandle lowInput;
        RenderGraphHandle highInput;
        RenderGraphHandle output;
    };

    eastl::string name = fmt::format("Bloom UpsampleMip{}({}x{})", mip, m_mipSize[mip].x, m_mipSize[mip].y).c_str();

    auto upsample_pass = pRenderGraph->AddPass<UpsamplePassData>(name,
        [&](UpsamplePassData& data, RenderGraphBuilder& builder)
        {
            data.lowInput = builder.Read(lowInput, GfxResourceState::ShaderResourceNonPS);
            data.highInput = builder.Read(highInput, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_mipSize[mip].x;
            desc.height = m_mipSize[mip].y;
            desc.format = GfxFormat::R11G11B10F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.output = builder.Create<RenderGraphTexture>(desc, name);
            data.output = builder.Write(data.output, GfxResourceState::UnorderedAccess);
        },
        [=](const UpsamplePassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* highInput = (RenderGraphTexture*)pRenderGraph->GetResource(data.highInput);
            RenderGraphTexture* lowInput = (RenderGraphTexture*)pRenderGraph->GetResource(data.lowInput);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            Upsample(pCommandList, highInput->GetSRV(), lowInput->GetSRV(), output->GetUAV(), mip);
        });

    return upsample_pass->output;
}

void Bloom::Downsample(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t mip)
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
    constants.inputTexture = input->GetHeapIndex();
    constants.outputTexture = output->GetHeapIndex();
    constants.inputPixelSize = float2(1.0f / input_width, 1.0f / input_height);
    constants.outputPixelSize = float2(1.0f / output_width, 1.0f / output_height);
    constants.threshold = m_threshold;

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((output_width + 7) / 8, (output_height + 7) / 8, 1);
}

void Bloom::Upsample(IGfxCommandList* pCommandList, IGfxDescriptor* highInput, IGfxDescriptor* lowInput, IGfxDescriptor* output, uint32_t mip)
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
    constants.inputLowTexture = lowInput->GetHeapIndex();
    constants.inputHighTexture = highInput->GetHeapIndex();
    constants.outputTexture = output->GetHeapIndex();
    constants.inputPixelSize = float2(1.0f / input_width, 1.0f / input_height);
    constants.outputPixelSize = float2(1.0f / output_width, 1.0f / output_height);

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));
    pCommandList->Dispatch((output_width + 7) / 8, (output_height + 7) / 8, 1);
}
