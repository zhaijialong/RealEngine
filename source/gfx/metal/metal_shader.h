#pragma once

#include "../gfx_shader.h"

class MetalShader : public IGfxShader
{
public:
    virtual void* GetHandle() const override;
    virtual bool SetShaderData(const uint8_t* data, uint32_t data_size) override;
};