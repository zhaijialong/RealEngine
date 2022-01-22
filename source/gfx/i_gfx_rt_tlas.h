#pragma once

#include "i_gfx_resource.h"

class IGfxRayTracingTLAS : public IGfxResource
{
public:
    const GfxRayTracingTLASDesc& GetDesc() const { return m_desc; }

protected:
    GfxRayTracingTLASDesc m_desc;
};