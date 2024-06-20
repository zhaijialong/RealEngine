#pragma once

#include "../gfx_rt_blas.h"

class MockDevice;

class MockRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    MockRayTracingBLAS(MockDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~MockRayTracingBLAS();

    bool Create();

    virtual void* GetHandle() const override;
};