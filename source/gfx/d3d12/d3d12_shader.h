#pragma once

#include "../i_gfx_shader.h"
#include <vector>

class D3D12Device;

class D3D12Shader : public IGfxShader
{
public:
    D3D12Shader(D3D12Device* pDevice, const GfxShaderDesc& desc, const std::vector<uint8_t> data, const std::string& name);

    virtual void* GetHandle() const override { return nullptr; }

private:
    std::vector<uint8_t> m_data;
};