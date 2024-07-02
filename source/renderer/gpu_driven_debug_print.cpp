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
    psoDesc.cs = pRenderer->GetShader("debug_print.hlsl", "build_command", GfxShaderType::CS);
    m_pBuildCommandPSO = pRenderer->GetPipelineState(psoDesc, "GpuDrivenDebugPrint buildCommand PSO");

    GfxMeshShadingPipelineDesc drawPSODesc;
    drawPSODesc.ms = pRenderer->GetShader("debug_print.hlsl", "main_ms", GfxShaderType::MS);
    drawPSODesc.ps = pRenderer->GetShader("debug_print.hlsl", "main_ps", GfxShaderType::PS);
    drawPSODesc.depthstencil_state.depth_write = false;
    drawPSODesc.rt_format[0] = pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
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

    pCommandList->BufferBarrier(m_pTextCounterBuffer->GetBuffer(), GfxAccessComputeSRV | GfxAccessVertexShaderSRV, GfxAccessCopyDst);

    //reset count to 0
    pCommandList->WriteBuffer(m_pTextCounterBuffer->GetBuffer(), 0, 0);

    pCommandList->BufferBarrier(m_pTextCounterBuffer->GetBuffer(), GfxAccessCopyDst, GfxAccessMaskUAV);
    pCommandList->BufferBarrier(m_pTextBuffer->GetBuffer(), GfxAccessVertexShaderSRV, GfxAccessMaskUAV);
}

void GpuDrivenDebugPrint::PrepareForDraw(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugPrint build command");

    pCommandList->BufferBarrier(m_pTextCounterBuffer->GetBuffer(), GfxAccessMaskUAV, GfxAccessComputeSRV | GfxAccessVertexShaderSRV);
    pCommandList->BufferBarrier(m_pDrawArugumentsBuffer->GetBuffer(), GfxAccessIndirectArgs, GfxAccessComputeUAV);

    pCommandList->SetPipelineState(m_pBuildCommandPSO);

    uint32_t root_consts[2] = { m_pDrawArugumentsBuffer->GetUAV()->GetHeapIndex(), m_pTextCounterBuffer->GetSRV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, root_consts, sizeof(root_consts));
    pCommandList->Dispatch(1, 1, 1);

    pCommandList->BufferBarrier(m_pDrawArugumentsBuffer->GetBuffer(), GfxAccessComputeUAV, GfxAccessIndirectArgs);
    pCommandList->BufferBarrier(m_pTextBuffer->GetBuffer(), GfxAccessMaskUAV, GfxAccessVertexShaderSRV);
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
    eastl::string font_file = Engine::GetInstance()->GetAssetPath() + "fonts/Roboto-Medium.ttf";

    eastl::vector<uint8_t> ttf_buffer(1 << 20);

    FILE* file = fopen(font_file.c_str(), "rb");
    RE_ASSERT(file != nullptr);
    fread(ttf_buffer.data(), 1, 1 << 20, file);
    fclose(file);

    const uint32_t char_count = 128;
    const uint32_t width = 256;
    const uint32_t height = 256;

    eastl::vector<unsigned char> bitmap(width * height);
    eastl::vector<stbtt_bakedchar> cdata(char_count);

    stbtt_BakeFontBitmap(ttf_buffer.data(), 0, 16.0, bitmap.data(), width, height, 0, char_count, cdata.data());

    m_pFontTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::R8UNORM, 0, "GpuDrivenDebugPrint::m_pFontTexture"));
    m_pRenderer->UploadTexture(m_pFontTexture->GetTexture(), bitmap.data());

    //packed version of stbtt_bakedchar
    struct BakedChar
    {
        uint8_t x0;
        uint8_t y0;
        uint8_t x1;
        uint8_t y1;

        float xoff;
        float yoff;
        float xadvance;
    };

    eastl::vector<BakedChar> packed_cdata(char_count);
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
