#pragma once

#include "i_gfx_resource.h"

class IGfxPipelineState : public IGfxResource
{
public:
    GfxPipelineType GetType() const { return m_type; }

protected:
    GfxPipelineType m_type;
};