#include "mock_shader.h"
#include "mock_device.h"
#include "xxHash/xxhash.h"

MockShader::MockShader(MockDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

void* MockShader::GetHandle() const
{
    return nullptr;
}

bool MockShader::Create(eastl::span<uint8_t> data)
{
    m_data.resize(data.size());
    memcpy(m_data.data(), data.data(), data.size());

    m_hash = XXH3_64bits(data.data(), data.size());

    return true;
}
