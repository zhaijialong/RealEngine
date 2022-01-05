#pragma once

#include "resource/raw_buffer.h"

class Renderer;
namespace D3D12MA { class VirtualBlock; }

class GpuScene
{
public:
    GpuScene(Renderer* pRenderer);
    ~GpuScene();

    uint32_t Allocate(uint32_t size, uint32_t alignment);
    void Free(uint32_t address);

    uint32_t AllocateConstantBuffer(uint32_t size);

    void ClearSceneBuffer();
    void ResetSceneConstants();

    IGfxBuffer* GetSceneBuffer() const { return m_pSceneBuffer->GetBuffer(); }
    IGfxDescriptor* GetSceneBufferSRV() const { return m_pSceneBuffer->GetSRV(); }

    IGfxBuffer* GetSceneConstantBuffer() const;
    IGfxDescriptor* GetSceneConstantSRV() const;

private:
    Renderer* m_pRenderer = nullptr;

    std::unique_ptr<RawBuffer> m_pSceneBuffer;
    D3D12MA::VirtualBlock* m_pSceneBufferAllocator = nullptr;

    std::unique_ptr<RawBuffer> m_pConstantBuffer[GFX_MAX_INFLIGHT_FRAMES];
    uint32_t m_nConstantBufferOffset = 0;
};