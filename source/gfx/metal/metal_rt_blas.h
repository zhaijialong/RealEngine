#pragma once

#include "../gfx_rt_blas.h"

class MetalDevice;

class MetalRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    MetalRayTracingBLAS(MetalDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~MetalRayTracingBLAS();

    bool Create();
    
    virtual void* GetHandle() const override;
};
