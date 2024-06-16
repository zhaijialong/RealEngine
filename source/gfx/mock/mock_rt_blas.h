#pragma once

#include "../gfx_rt_blas.h"

class MockRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    virtual void* GetHandle() const override;
};