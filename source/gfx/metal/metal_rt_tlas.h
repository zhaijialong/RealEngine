#pragma once

#include "../gfx_rt_tlas.h"

class MetalRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    virtual void* GetHandle() const override;
};