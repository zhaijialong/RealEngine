#include "gpu_debug_line.h"
#include "renderer.h"

#define MAX_VERTEX_COUNT (1024 * 1024)

GpuDebugLine::GpuDebugLine(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("debug_line.hlsl", "vs_main", "vs_6_6", {});
    psoDesc.ps = pRenderer->GetShader("debug_line.hlsl", "ps_main", "ps_6_6", {});
    psoDesc.rasterizer_state.line_aa = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;
    psoDesc.primitive_type = GfxPrimitiveType::LineList;
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "GpuDebugLine::m_pPSO");

    GfxDrawCommand command = {};
    command.instance_count = 1;
    m_pDrawArugumentsBuffer.reset(pRenderer->CreateRawBuffer(&command, sizeof(GfxDrawCommand), "GpuDebugLine::m_pDrawArugumentsBuffer", GfxMemoryType::GpuOnly, true));

    const uint32_t lineVertexStride = 16; //float3 pos + uint color
    m_pLineVertexBuffer.reset(pRenderer->CreateStructuredBuffer(nullptr, 16, MAX_VERTEX_COUNT, "GpuDebugLine::m_pLineVertexBuffer", GfxMemoryType::GpuOnly, true));
}

void GpuDebugLine::Clear(IGfxCommandList* pCommandList)
{
    pCommandList->ResourceBarrier(m_pDrawArugumentsBuffer->GetBuffer(), 0, GfxResourceState::IndirectArg, GfxResourceState::CopyDst);

    //reset vertex_count to 0
    pCommandList->WriteBuffer(m_pDrawArugumentsBuffer->GetBuffer(), 0, 0);

    pCommandList->ResourceBarrier(m_pDrawArugumentsBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);
    pCommandList->ResourceBarrier(m_pLineVertexBuffer->GetBuffer(), 0, GfxResourceState::ShaderResourceNonPS, GfxResourceState::UnorderedAccess);
}

void GpuDebugLine::BarrierForDraw(IGfxCommandList* pCommandList)
{
    pCommandList->ResourceBarrier(m_pDrawArugumentsBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::IndirectArg);
    pCommandList->ResourceBarrier(m_pLineVertexBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceNonPS);
}

void GpuDebugLine::Draw(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GpuDebugLine");

    pCommandList->SetPipelineState(m_pPSO);
    pCommandList->DrawIndirect(m_pDrawArugumentsBuffer->GetBuffer(), 0);
}
