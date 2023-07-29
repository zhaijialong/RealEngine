#pragma once

#include "../gfx_rt_blas.h"

class VulkanRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    virtual void* GetHandle() const override;
};