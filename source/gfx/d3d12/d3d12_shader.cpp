#include "d3d12_shader.h"
#include "d3d12_device.h"
#include "xxHash/xxhash.h"

D3D12Shader::D3D12Shader(D3D12Device* pDevice, const GfxShaderDesc& desc, const std::vector<uint8_t> data, const std::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_data = data;
    m_name = name;

    m_hash = XXH3_64bits(data.data(), data.size());
}