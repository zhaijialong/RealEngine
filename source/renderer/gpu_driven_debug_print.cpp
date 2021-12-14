#include "gpu_driven_debug_print.h"
#include "renderer.h"
#include "core/engine.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#define MAX_TEXT_COUNT (64 * 1024)

GpuDrivenDebugPrint::GpuDrivenDebugPrint(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("debug_print.hlsl", "build_command", "cs_6_6", {});
    m_pBuildCommandPSO = pRenderer->GetPipelineState(psoDesc, "GpuDrivenDebugPrint buildCommand PSO");

    GfxMeshShadingPipelineDesc drawPSODesc;
    drawPSODesc.ms = pRenderer->GetShader("debug_print.hlsl", "main_ms", "ms_6_6", {});
    drawPSODesc.ps = pRenderer->GetShader("debug_print.hlsl", "main_ps", "ps_6_6", {});
    drawPSODesc.rt_format[0] = pRenderer->GetSwapchain()->GetBackBuffer()->GetDesc().format;
    drawPSODesc.blend_state[0].blend_enable = true;
    drawPSODesc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
    drawPSODesc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
    drawPSODesc.blend_state[0].alpha_src = GfxBlendFactor::One;
    drawPSODesc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
    m_pDrawPSO = pRenderer->GetPipelineState(drawPSODesc, "GpuDrivenDebugPrint PSO");

    uint32_t count = 0;
    m_pTextCounterBuffer.reset(pRenderer->CreateRawBuffer(&count, sizeof(count), "GpuDrivenDebugPrint::m_pTextCounterBuffer", GfxMemoryType::GpuOnly, true));
    m_pTextBuffer.reset(pRenderer->CreateStructuredBuffer(nullptr, 16, MAX_TEXT_COUNT, "GpuDrivenDebugPrint::m_pTextBuffer", GfxMemoryType::GpuOnly, true));

    GfxDispatchCommand command = { 0, 1, 1 };
    m_pDrawArugumentsBuffer.reset(pRenderer->CreateRawBuffer(&command, sizeof(command), "GpuDrivenDebugPrint::m_pDrawArugumentsBuffer", GfxMemoryType::GpuOnly, true));

    CreateFontTexture();
}

void GpuDrivenDebugPrint::Clear(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugPrint clear");

    pCommandList->ResourceBarrier(m_pTextCounterBuffer->GetBuffer(), 0, GfxResourceState::ShaderResourceNonPS, GfxResourceState::CopyDst);

    //reset count to 0
    pCommandList->WriteBuffer(m_pTextCounterBuffer->GetBuffer(), 0, 0);

    pCommandList->ResourceBarrier(m_pTextCounterBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);
    pCommandList->ResourceBarrier(m_pTextBuffer->GetBuffer(), 0, GfxResourceState::ShaderResourcePS, GfxResourceState::UnorderedAccess);
}

void GpuDrivenDebugPrint::PrepareForDraw(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugPrint build command");

    pCommandList->ResourceBarrier(m_pTextCounterBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceNonPS);
    pCommandList->ResourceBarrier(m_pDrawArugumentsBuffer->GetBuffer(), 0, GfxResourceState::IndirectArg, GfxResourceState::UnorderedAccess);

    pCommandList->SetPipelineState(m_pBuildCommandPSO);

    uint32_t root_consts[2] = { m_pDrawArugumentsBuffer->GetUAV()->GetHeapIndex(), m_pTextCounterBuffer->GetSRV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));
    pCommandList->Dispatch(1, 1, 1);

    pCommandList->ResourceBarrier(m_pDrawArugumentsBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::IndirectArg);
    pCommandList->ResourceBarrier(m_pTextBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourcePS);
}

void GpuDrivenDebugPrint::Draw(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugPrint");

    pCommandList->SetPipelineState(m_pDrawPSO);

    uint32_t root_consts[4] = { 0 , m_pTextCounterBuffer->GetSRV()->GetHeapIndex(), m_pTextBuffer->GetSRV()->GetHeapIndex(), m_pFontTexture->GetSRV()->GetHeapIndex() };
    pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

    pCommandList->DispatchMeshIndirect(m_pDrawArugumentsBuffer->GetBuffer(), 0);
}

void GpuDrivenDebugPrint::CreateFontTexture()
{
    std::string font_file = Engine::GetInstance()->GetAssetPath() + "fonts/Roboto-Medium.ttf";

    std::vector<uint8_t> ttf_buffer(1 << 20);

    FILE* file = fopen(font_file.c_str(), "rb");
    RE_ASSERT(file != nullptr);
    fread(ttf_buffer.data(), 1, 1 << 20, file);
    fclose(file);

    const uint32_t char_count = 128;
    const uint32_t width = 256;
    const uint32_t height = 256;

    std::vector<unsigned char> bitmap(width * height);
    std::vector<stbtt_bakedchar> cdata(char_count);

    stbtt_BakeFontBitmap(ttf_buffer.data(), 0, 16.0, bitmap.data(), width, height, 0, char_count, cdata.data());

    m_pFontTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UNORM, GfxTextureUsageShaderResource, "GpuDrivenDebugPrint::m_pFontTexture"));
    m_pRenderer->UploadTexture(m_pFontTexture->GetTexture(), bitmap.data());

    //packed version of stbtt_bakedchar
    struct BakedChar
    {
        union
        {
            struct
            {
                uint8_t x0;
                uint8_t y0;
                uint8_t x1;
                uint8_t y1;
            };
            uint32_t bbox;
        };

        float xoff;
        float yoff;
        float xadvance;
    };

    std::vector<BakedChar> packed_cdata(char_count);
    for (uint32_t i = 0; i < char_count; ++i)
    {
        packed_cdata[i].x0 = (uint8_t)cdata[i].x0;
        packed_cdata[i].y0 = (uint8_t)cdata[i].y0;
        packed_cdata[i].x1 = (uint8_t)cdata[i].x1;
        packed_cdata[i].y1 = (uint8_t)cdata[i].y1;
        packed_cdata[i].xoff = cdata[i].xoff;
        packed_cdata[i].yoff = cdata[i].yoff;
        packed_cdata[i].xadvance = cdata[i].xadvance;
    }

    m_pFontCharBuffer.reset(m_pRenderer->CreateStructuredBuffer(packed_cdata.data(), sizeof(BakedChar), char_count, "GpuDrivenDebugPrint::m_pFontCharBuffer"));
}
