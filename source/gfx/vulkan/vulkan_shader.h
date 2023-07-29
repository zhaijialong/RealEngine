#pragma once

#include "../gfx_shader.h"

class VulkanShader : public IGfxShader
{
public:
    virtual void* GetHandle() const override;
    virtual bool SetShaderData(const uint8_t* data, uint32_t data_size) override;
};