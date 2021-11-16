#pragma once

#include "gfx/gfx.h"
#include "resource/raw_buffer.h"
#include "resource/structured_buffer.h"

class Renderer;

class GpuDebugLine
{
public:
    GpuDebugLine(Renderer* pRenderer);

    void Clear(IGfxCommandList* pCommandList);
    void BarrierForDraw(IGfxCommandList* pCommandList);
    void Draw(IGfxCommandList* pCommandList);

    IGfxDescriptor* GetVertexBufferSRV() const { return m_pLineVertexBuffer->GetSRV(); }
    IGfxDescriptor* GetVertexBufferUAV() const { return m_pLineVertexBuffer->GetUAV(); }
    IGfxDescriptor* GetArugumentsBufferUAV() const { return m_pDrawArugumentsBuffer->GetUAV(); }

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    std::unique_ptr<RawBuffer> m_pDrawArugumentsBuffer;
    std::unique_ptr<StructuredBuffer> m_pLineVertexBuffer;
};