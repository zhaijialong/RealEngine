#pragma once

#include "resource/raw_buffer.h"
#include "utils/math.h"
#include "gpu_scene.hlsli"

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

    uint32_t AddInstance(const InstanceData& data);
    uint32_t GetInstanceCount() const { return (uint32_t)m_instanceData.size(); }

    void Update();
    void ResetFrameData();

    IGfxBuffer* GetSceneBuffer() const { return m_pSceneBuffer->GetBuffer(); }
    IGfxDescriptor* GetSceneBufferSRV() const { return m_pSceneBuffer->GetSRV(); }

    IGfxBuffer* GetSceneConstantBuffer() const;
    IGfxDescriptor* GetSceneConstantSRV() const;

    uint32_t GetInstanceDataAddress() const { return m_instanceDataAddress; }

private:
    Renderer* m_pRenderer = nullptr;

    std::vector<InstanceData> m_instanceData;
    uint32_t m_instanceDataAddress = 0;

    std::unique_ptr<RawBuffer> m_pSceneBuffer;
    D3D12MA::VirtualBlock* m_pSceneBufferAllocator = nullptr;

    std::unique_ptr<RawBuffer> m_pConstantBuffer[GFX_MAX_INFLIGHT_FRAMES]; //todo : change to gpu memory, and only update dirty regions
    uint32_t m_nConstantBufferOffset = 0;
};