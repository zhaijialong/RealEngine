#include "d3d12_shader.h"
#include "d3d12_device.h"
#include "xxHash/xxhash.h"

D3D12Shader::D3D12Shader(D3D12Device* pDevice, const GfxShaderDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

bool D3D12Shader::Create(eastl::span<uint8_t> data)
{
    m_data.resize(data.size());
    memcpy(m_data.data(), data.data(), data.size());

    m_hash = XXH3_64bits(data.data(), data.size());

    return true;
}
