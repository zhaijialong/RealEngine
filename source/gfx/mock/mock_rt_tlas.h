#pragma once

#include "../gfx_rt_tlas.h"

class MockRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    virtual void* GetHandle() const override;
};