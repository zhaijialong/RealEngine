#pragma once

#include "i_gfx_resource.h"

class IGfxShader : public IGfxResource
{
public:
    const GfxShaderDesc& GetDesc() const { return m_desc; }

    uint64_t GetHash() const { return m_hash; }

protected:
    GfxShaderDesc m_desc = {};
    uint64_t m_hash = 0;
};