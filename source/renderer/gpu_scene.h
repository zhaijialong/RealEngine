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
    void Clear();

    IGfxBuffer* GetSceneBuffer() const { return m_pSceneBuffer->GetBuffer(); }
    IGfxDescriptor* GetSceneBufferSRV() const { return m_pSceneBuffer->GetSRV(); }

private:
    Renderer* m_pRenderer = nullptr;

    std::unique_ptr<RawBuffer> m_pSceneBuffer;
    D3D12MA::VirtualBlock* m_pSceneBufferAllocator = nullptr;
};