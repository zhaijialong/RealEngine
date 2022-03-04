#pragma once

#include "resource/typed_buffer.h"

class Renderer;

class GpuDrivenStats
{
public:
    GpuDrivenStats(Renderer* pRenderer);

    void Clear(IGfxCommandList* pCommandList);
    void Draw(IGfxCommandList* pCommandList);

    IGfxDescriptor* GetStatsBufferUAV() const { return m_pStatsBuffer->GetUAV(); }

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<TypedBuffer> m_pStatsBuffer;
};