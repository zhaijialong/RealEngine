#pragma once

#include "../gfx_shader.h"

class WebGPUDevice;

class WebGPUShader : public IGfxShader
{
public:
    WebGPUShader(WebGPUDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name);

    virtual void* GetHandle() const override;
    virtual bool Create(eastl::span<uint8_t> data) override;

private:
    eastl::vector<uint8_t> m_data;
};