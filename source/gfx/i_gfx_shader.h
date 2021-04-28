#pragma once

#include "i_gfx_resource.h"

class IGfxShader : public IGfxResource
{
public:
    const GfxShaderDesc& GetDesc() const { return m_desc; }

protected:
    GfxShaderDesc m_desc = {};
};