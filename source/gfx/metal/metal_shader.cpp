#include "metal_shader.h"
#include "metal_device.h"
#include "xxHash/xxhash.h"

MetalShader::MetalShader(MetalDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

void* MetalShader::GetHandle() const
{
    return nullptr;
}

bool MetalShader::Create(eastl::span<uint8_t> data)
{
    m_hash = XXH3_64bits(data.data(), data.size());

    return true;
}
