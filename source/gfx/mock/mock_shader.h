#pragma once

#include "../gfx_shader.h"

class MockShader : public IGfxShader
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create(eastl::span<uint8_t> data) override;
};