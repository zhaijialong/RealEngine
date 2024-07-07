#pragma once

#include "metal_utils.h"
#include "../gfx_shader.h"

class MetalDevice;

class MetalShader : public IGfxShader
{
public:
    MetalShader(MetalDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name);
    ~MetalShader();
    
    virtual void* GetHandle() const override { return m_pFunction; }
    virtual bool Create(eastl::span<uint8_t> data) override;
    
private:
    MTL::Function* m_pFunction = nullptr;
};
