#pragma once

#include "resource/raw_buffer.h"
#include "utils/math.h"
#include "OffsetAllocator/offsetAllocator.hpp"
#include "gpu_scene.hlsli"

class Renderer;

class GpuScene
{
public:
    GpuScene(Renderer* pRenderer);
    ~GpuScene();

    OffsetAllocator::Allocation AllocateStaticBuffer(uint32_t size);
    void FreeStaticBuffer(OffsetAllocator::Allocation allocation);

    OffsetAllocator::Allocation AllocateAnimationBuffer(uint32_t size);
    void FreeAnimationBuffer(OffsetAllocator::Allocation allocation);

    uint32_t AllocateConstantBuffer(uint32_t size);

    uint32_t AddInstance(const InstanceData& data, IGfxRayTracingBLAS* blas, GfxRayTracingInstanceFlag flags);
    uint32_t GetInstanceCount() const { return (uint32_t)m_instanceData.size(); }

    uint32_t AddLocalLight(const LocalLightData& data);
    uint32_t GetLocalLightCount() const { return (uint32_t)m_localLightsData.size(); }
    const LocalLightData* GetLocalLights() const { return m_localLightsData.data(); }

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
    uint32_t GetLocalLightsDataAddress() const { return m_localLightsDataAddress; }

    IGfxDescriptor* GetRayTracingTLASSRV() const { return m_pSceneTLASSRV.get(); }

private:
    Renderer* m_pRenderer = nullptr;

    eastl::vector<InstanceData> m_instanceData;
    uint32_t m_instanceDataAddress = 0;

    eastl::vector<LocalLightData> m_localLightsData;
    uint32_t m_localLightsDataAddress = 0;

    eastl::unique_ptr<RawBuffer> m_pSceneStaticBuffer;
    eastl::unique_ptr<OffsetAllocator::Allocator> m_pSceneStaticBufferAllocator;

    eastl::unique_ptr<RawBuffer> m_pSceneAnimationBuffer;
    eastl::unique_ptr<OffsetAllocator::Allocator> m_pSceneAnimationBufferAllocator;

    eastl::unique_ptr<RawBuffer> m_pConstantBuffer[GFX_MAX_INFLIGHT_FRAMES]; //todo : change to gpu memory, and only update dirty regions
    uint32_t m_nConstantBufferOffset = 0;

    eastl::unique_ptr<IGfxRayTracingTLAS> m_pSceneTLAS;
    eastl::unique_ptr<IGfxDescriptor> m_pSceneTLASSRV;
    eastl::vector<GfxRayTracingInstance> m_raytracingInstances;
};