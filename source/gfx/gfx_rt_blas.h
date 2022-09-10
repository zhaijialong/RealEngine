#pragma once

#include "gfx_resource.h"

class IGfxRayTracingBLAS : public IGfxResource
{
public:
    const GfxRayTracingBLASDesc& GetDesc() const { return m_desc; }

protected:
    GfxRayTracingBLASDesc m_desc;
};