#pragma once

#include "../gfx_rt_blas.h"

class WebGPUDevice;

class WebGPURayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    WebGPURayTracingBLAS(WebGPUDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~WebGPURayTracingBLAS();

    bool Create();

    virtual void* GetHandle() const override;
};