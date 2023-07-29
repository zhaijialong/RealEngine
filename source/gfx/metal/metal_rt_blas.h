#pragma once

#include "../gfx_rt_blas.h"

class MetalRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    virtual void* GetHandle() const override;
};