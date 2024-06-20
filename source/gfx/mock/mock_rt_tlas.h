#pragma once

#include "../gfx_rt_tlas.h"

class MockDevice;

class MockRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    MockRayTracingTLAS(MockDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name);
    ~MockRayTracingTLAS();

    bool Create();

    virtual void* GetHandle() const override;
};