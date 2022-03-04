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

    uint32_t AllocateStaticBuffer(uint32_t size, uint32_t alignment);
    void FreeStaticBuffer(uint32_t address);

    uint32_t AllocateAnimationBuffer(uint32_t size, uint32_t alignment);
    void FreeAnimationBuffer(uint32_t address);

    uint32_t AllocateConstantBuffer(uint32_t size);

    uint32_t AddInstance(const InstanceData& data, IGfxRayTracingBLAS* blas, GfxRayTracingInstanceFlag flags);
    uint32_t GetInstanceCount() const { return (uint32_t)m_instanceData.size(); }

    void Update();
    void BuildRayTracingAS(IGfxCommandList* pCommandList);
    void ResetFrameData();

    void BeginAnimationUpdate(IGfxCommandList* pCommandList);
    void EndAnimationUpdate(IGfxCommandList* pCommandList);

    IGfxBuffer* GetSceneStaticBuffer() const { return m_pSceneStaticBuffer->GetBuffer(); }
    IGfxDescriptor* GetSceneStaticBufferSRV() const { return m_pSceneStaticBuffer->GetSRV(); }

    IGfxBuffer* GetSceneAnimationBuffer() const { return m_pSceneAnimationBuffer->GetBuffer(); }
    IGfxDescriptor* GetSceneAnimationBufferSRV() const { return m_pSceneAnimationBuffer->GetSRV(); }
    IGfxDescriptor* GetSceneAnimationBufferUAV() const { return m_pSceneAnimationBuffer->GetUAV(); }

    IGfxBuffer* GetSceneConstantBuffer() const;
    IGfxDescriptor* GetSceneConstantSRV() const;

    uint32_t GetInstanceDataAddress() const { return m_instanceDataAddress; }

    IGfxDescriptor* GetRayTracingTLASSRV() const { return m_pSceneTLASSRV.get(); }

private:
    Renderer* m_pRenderer = nullptr;

    eastl::vector<InstanceData> m_instanceData;
    uint32_t m_instanceDataAddress = 0;

    eastl::unique_ptr<RawBuffer> m_pSceneStaticBuffer;
    D3D12MA::VirtualBlock* m_pSceneStaticBufferAllocator = nullptr;

    eastl::unique_ptr<RawBuffer> m_pSceneAnimationBuffer;
    D3D12MA::VirtualBlock* m_pSceneAnimationBufferAllocator = nullptr;

    eastl::unique_ptr<RawBuffer> m_pConstantBuffer[GFX_MAX_INFLIGHT_FRAMES]; //todo : change to gpu memory, and only update dirty regions
    uint32_t m_nConstantBufferOffset = 0;

    eastl::unique_ptr<IGfxRayTracingTLAS> m_pSceneTLAS;
    eastl::unique_ptr<IGfxDescriptor> m_pSceneTLASSRV;
    eastl::vector<GfxRayTracingInstance> m_raytracingInstances;
};