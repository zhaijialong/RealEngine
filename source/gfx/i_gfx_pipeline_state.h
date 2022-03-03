#pragma once

#include "i_gfx_resource.h"

class IGfxPipelineState : public IGfxResource
{
public:
    GfxPipelineType GetType() const { return m_type; }

    virtual bool Create() = 0;

protected:
    GfxPipelineType m_type;
};