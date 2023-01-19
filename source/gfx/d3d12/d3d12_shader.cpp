#include "d3d12_shader.h"
#include "d3d12_device.h"
#include "xxHash/xxhash.h"

D3D12Shader::D3D12Shader(D3D12Device* pDevice, const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;

    SetShaderData(data.data(), (uint32_t)data.size());
}

bool D3D12Shader::SetShaderData(const uint8_t* data, uint32_t data_size)
{
    m_data.resize(data_size);
    memcpy(m_data.data(), data, data_size);

    m_hash = XXH3_64bits(data, data_size);

    return true;
}
