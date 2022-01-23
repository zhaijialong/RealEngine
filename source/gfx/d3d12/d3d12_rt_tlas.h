#pragma once

#include "d3d12_header.h"
#include "../i_gfx_rt_tlas.h"

class D3D12Device;

class D3D12RayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    D3D12RayTracingTLAS(D3D12Device* pDevice, const GfxRayTracingTLASDesc& desc, const std::string& name);
    ~D3D12RayTracingTLAS();

    virtual void* GetHandle() const override { return m_pASBuffer; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return m_pASBuffer->GetGPUVirtualAddress(); }

    bool Create();
    void GetBuildDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, const GfxRayTracingInstance* instances, uint32_t instance_count);

private:
    ID3D12Resource* m_pASBuffer = nullptr;
    ID3D12Resource* m_pScratchBuffer = nullptr;

    ID3D12Resource* m_pInstanceBuffer = nullptr;
    void* m_pInstanceBufferCpuAddress = nullptr;
    uint32_t m_nInstanceBufferSize = 0;
    uint32_t m_nCurrentInstanceBufferOffset = 0;
};