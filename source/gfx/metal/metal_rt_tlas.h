#pragma once

#include "metal_utils.h"
#include "../gfx_rt_tlas.h"

class MetalDevice;

class MetalRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    MetalRayTracingTLAS(MetalDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name);
    ~MetalRayTracingTLAS();

    bool Create();
    void UpdateInstance(const GfxRayTracingInstance* instances, uint32_t instance_count);
    MTL::AccelerationStructure* GetAccelerationStructure() const { return m_pAccelerationStructure; }
    MTL::InstanceAccelerationStructureDescriptor* GetDescriptor() const  { return m_pDescriptor; }
    MTL::Buffer* GetScratchBuffer() const { return m_pScratchBuffer; }
    
    virtual void* GetHandle() const override { return m_pAccelerationStructure; }

private:
    MTL::AccelerationStructure* m_pAccelerationStructure = nullptr;
    MTL::InstanceAccelerationStructureDescriptor* m_pDescriptor = nullptr;
    MTL::Buffer* m_pScratchBuffer = nullptr;
    MTL::Buffer* m_pInstanceBuffer = nullptr;
    uint32_t m_instanceBufferSize = 0;
    uint32_t m_currentInstanceBufferOffset = 0;
    void* m_instanceBufferCpuAddress = nullptr;
};
