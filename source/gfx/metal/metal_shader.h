#pragma once

#include "../gfx_shader.h"

class MetalDevice;

class MetalShader : public IGfxShader
{
public:
    MetalShader(MetalDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name);
    
    virtual void* GetHandle() const override;
    virtual bool Create(eastl::span<uint8_t> data) override;
};
