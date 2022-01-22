#pragma once

#include "d3d12_header.h"
#include "../i_gfx_rt_tlas.h"

class D3D12Device;

class D3D12RayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    D3D12RayTracingTLAS(D3D12Device* pDevice, const GfxRayTracingTLASDesc& desc, const std::string& name);

    virtual void* GetHandle() const override { return nullptr; }

    bool Create();
};