#include "tonemapper.h"
#include "../renderer.h"
#include "utils/gui_util.h"

Tonemapper::Tonemapper(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RGHandle Tonemapper::AddPass(RenderGraph* pRenderGraph, RGHandle inputHandle, RGHandle exposure, RGHandle bloom, float bloom_intensity, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "Tone Mapping",
        [&]()
        {
            ImGui::Combo("Mode##Tonemapper", (int*)&m_mode, "Neutral\0ACES\0Tony McMapface\0AgX\0\0");
            ImGui::Checkbox("Enable Dither##Tonemapper", &m_bEnableDither);
        });

    struct TonemapPassData
    {
        RGHandle hdrRT;
        RGHandle exposure;
        RGHandle bloom;

        RGHandle outLdrRT;
    };

    auto tonemap_pass = pRenderGraph->AddPass<TonemapPassData>("ToneMapping", RenderPassType::Compute,
        [&](TonemapPassData& data, RGBuilder& builder)
        {
            data.hdrRT = builder.Read(inputHandle);
            data.exposure = builder.Read(exposure);

            if (bloom.IsValid())
            {
                data.bloom = builder.Read(bloom);
            }

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA8SRGB;
            data.outLdrRT = builder.Create<RGTexture>(desc, "ToneMapping Output");
            data.outLdrRT = builder.Write(data.outLdrRT);
        },
        [=](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            Render(pCommandList,
                pRenderGraph->GetTexture(data.hdrRT), 
                pRenderGraph->GetTexture(data.exposure),
                pRenderGraph->GetTexture(data.outLdrRT), 
                pRenderGraph->GetTexture(data.bloom),
                bloom_intensity, width, height);
        });

    return tonemap_pass->outLdrRT;
}

void Tonemapper::Render(IGfxCommandList* pCommandList, RGTexture* pHdrSRV, RGTexture* exposure, RGTexture* pLdrUAV, RGTexture* bloom, float bloom_intensity, uint32_t width, uint32_t height)
{
    eastl::vector<eastl::string> defines;

    switch (m_mode)
    {
    case TonemappingMode::Neutral:
        defines.push_back("NEUTRAL=1");
        break;
    case TonemappingMode::ACES:
        defines.push_back("ACES=1");
        break;
    case TonemappingMode::TonyMcMapface:
        defines.push_back("TONY_MC_MAPFACE=1");
        break;
    case TonemappingMode::AgX:
        defines.push_back("AGX=1");
        break;
    default:
        break;
    }

    if (bloom)
    {
        defines.push_back("BLOOM=1");
    }

    if (m_bEnableDither)
    {
        defines.push_back("DITHER=1");
    }

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = m_pRenderer->GetShader("tone_mapping.hlsl", "cs_main", GfxShaderType::CS, defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(psoDesc, "ToneMapping PSO");

    pCommandList->SetPipelineState(pso);

    struct Constants
    {
        uint hdrTexture;
        uint ldrTexture;
        uint exposureTexture;
        uint bloomTexture;
        float2 pixelSize;
        float2 bloomPixelSize;
        float bloomIntensity;
    };
    
    Constants constants;
    constants.hdrTexture = pHdrSRV->GetSRV()->GetHeapIndex();
    constants.ldrTexture = pLdrUAV->GetUAV()->GetHeapIndex();
    constants.exposureTexture = exposure->GetSRV()->GetHeapIndex();
    constants.bloomTexture = bloom ? bloom->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    constants.bloomIntensity = bloom_intensity;
    constants.pixelSize = float2(1.0f / width, 1.0f / height);

    uint32_t bloom_width = (width + 1) >> 1;
    uint32_t bloom_height = (height + 1) >> 1;
    constants.bloomPixelSize = float2(1.0f / bloom_width, 1.0f / bloom_height);

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
