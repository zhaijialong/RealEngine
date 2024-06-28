#pragma once

#include "../gfx_rt_tlas.h"

class MetalDevice;

class MetalRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    MetalRayTracingTLAS(MetalDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name);
    ~MetalRayTracingTLAS();

    bool Create();
    
    virtual void* GetHandle() const override;
};
