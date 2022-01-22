#pragma once

#include "d3d12_header.h"
#include "../i_gfx_rt_blas.h"

class D3D12Device;

class D3D12RayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    D3D12RayTracingBLAS(D3D12Device* pDevice, const GfxRayTracingBLASDesc& desc, const std::string& name);

    virtual void* GetHandle() const override { return nullptr; }

    bool Create();
};