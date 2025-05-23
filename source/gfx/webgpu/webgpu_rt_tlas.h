#pragma once

#include "../gfx_rt_tlas.h"

class WebGPUDevice;

class WebGPURayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    WebGPURayTracingTLAS(WebGPUDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name);
    ~WebGPURayTracingTLAS();

    bool Create();

    virtual void* GetHandle() const override;
};