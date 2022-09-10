#pragma once

#include "gfx_resource.h"

class IGfxShader : public IGfxResource
{
public:
    const GfxShaderDesc& GetDesc() const { return m_desc; }

    uint64_t GetHash() const { return m_hash; }

    virtual bool SetShaderData(const uint8_t* data, uint32_t data_size) = 0;

protected:
    GfxShaderDesc m_desc = {};
    uint64_t m_hash = 0;
};