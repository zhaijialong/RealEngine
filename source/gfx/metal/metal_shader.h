#pragma once

#include "metal_utils.h"
#include "metal_shader_reflection.h"
#include "../gfx_shader.h"

class MetalDevice;

class MetalShader : public IGfxShader
{
public:
    MetalShader(MetalDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name);
    ~MetalShader();
    
    const MetalShaderReflection& GetReflection() const { return m_reflection;}
    
    virtual void* GetHandle() const override { return m_pFunction; }
    virtual bool Create(eastl::span<uint8_t> data) override;
    
private:
    MTL::Function* m_pFunction = nullptr;
    MetalShaderReflection m_reflection;
};
