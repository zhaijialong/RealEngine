#pragma once

#include "d3d12_header.h"
#include "../gfx_rt_blas.h"

class D3D12Device;

class D3D12RayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    D3D12RayTracingBLAS(D3D12Device* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~D3D12RayTracingBLAS();

    virtual void* GetHandle() const override { return m_pASBuffer; }

    bool Create();
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* GetBuildDesc() const { return &m_buildDesc; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return m_pASBuffer->GetGPUVirtualAddress(); }

    void GetUpdateDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, D3D12_RAYTRACING_GEOMETRY_DESC& geometry, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset);

private:
    eastl::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geometries;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_buildDesc;

    ID3D12Resource* m_pASBuffer = nullptr;
    ID3D12Resource* m_pScratchBuffer = nullptr;
};