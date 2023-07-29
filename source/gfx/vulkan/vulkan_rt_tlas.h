#pragma once

#include "../gfx_rt_tlas.h"

class VulkanRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    virtual void* GetHandle() const override;
};