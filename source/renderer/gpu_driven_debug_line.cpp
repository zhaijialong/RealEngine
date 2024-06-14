#include "gpu_driven_debug_line.h"
#include "renderer.h"

#define MAX_VERTEX_COUNT (1024 * 1024)

GpuDrivenDebugLine::GpuDrivenDebugLine(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("debug_line.hlsl", "vs_main", "vs_6_6", {});
    psoDesc.ps = pRenderer->GetShader("debug_line.hlsl", "ps_main", "ps_6_6", {});
    //psoDesc.rasterizer_state.line_aa = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.rt_format[0] = pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
    psoDesc.depthstencil_format = GfxFormat::D32F;
    psoDesc.primitive_type = GfxPrimitiveType::LineList;
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "GpuDrivenDebugLine::m_pPSO");

    GfxDrawCommand command = {};
    command.instance_count = 1;
    m_pDrawArugumentsBuffer.reset(pRenderer->CreateRawBuffer(&command, sizeof(GfxDrawCommand), "GpuDrivenDebugLine::m_pDrawArugumentsBuffer", GfxMemoryType::GpuOnly, true));

    const uint32_t lineVertexStride = 16; //float3 pos + uint color
    m_pLineVertexBuffer.reset(pRenderer->CreateStructuredBuffer(nullptr, 16, MAX_VERTEX_COUNT, "GpuDrivenDebugLine::m_pLineVertexBuffer", GfxMemoryType::GpuOnly, true));
}

void GpuDrivenDebugLine::Clear(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugLine clear");

    pCommandList->BufferBarrier(m_pDrawArugumentsBuffer->GetBuffer(), GfxAccessIndirectArgs, GfxAccessCopyDst);

    //reset vertex_count to 0
    pCommandList->WriteBuffer(m_pDrawArugumentsBuffer->GetBuffer(), 0, 0);

    pCommandList->BufferBarrier(m_pDrawArugumentsBuffer->GetBuffer(), GfxAccessCopyDst, GfxAccessMaskUAV);
    pCommandList->BufferBarrier(m_pLineVertexBuffer->GetBuffer(), GfxAccessVertexShaderSRV, GfxAccessMaskUAV);
}

void GpuDrivenDebugLine::PrepareForDraw(IGfxCommandList* pCommandList)
{
    pCommandList->BufferBarrier(m_pDrawArugumentsBuffer->GetBuffer(), GfxAccessMaskUAV, GfxAccessIndirectArgs);
    pCommandList->BufferBarrier(m_pLineVertexBuffer->GetBuffer(), GfxAccessMaskUAV, GfxAccessVertexShaderSRV);
}

void GpuDrivenDebugLine::Draw(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDrivenDebugLine");

    pCommandList->SetPipelineState(m_pPSO);

    uint root_constants[1] = { GetVertexBufferSRV()->GetHeapIndex() };
    pCommandList->SetGraphicsConstants(0, root_constants, sizeof(root_constants));

    pCommandList->DrawIndirect(m_pDrawArugumentsBuffer->GetBuffer(), 0);
}
