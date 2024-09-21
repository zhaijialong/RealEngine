#pragma once

#include "metal_utils.h"
#include "../gfx_rt_blas.h"

class MetalDevice;

class MetalRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    MetalRayTracingBLAS(MetalDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~MetalRayTracingBLAS();

    bool Create();
    void UpdateVertexBuffer(IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset);
    MTL::AccelerationStructure* GetAccelerationStructure() const { return m_pAccelerationStructure; }
    MTL::PrimitiveAccelerationStructureDescriptor* GetDescriptor() const  { return m_pDescriptor; }
    MTL::Buffer* GetScratchBuffer() const { return m_pScratchBuffer; }
    
    virtual void* GetHandle() const override { return m_pAccelerationStructure; }
    
private:
    MTL::AccelerationStructure* m_pAccelerationStructure = nullptr;
    MTL::PrimitiveAccelerationStructureDescriptor* m_pDescriptor = nullptr;
    MTL::Buffer* m_pScratchBuffer = nullptr;
    
    eastl::vector<MTL::AccelerationStructureTriangleGeometryDescriptor*> m_geometries;
};
