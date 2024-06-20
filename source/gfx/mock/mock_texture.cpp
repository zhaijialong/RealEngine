#include "mock_texture.h"
#include "mock_device.h"
#include "../gfx.h"

MockTexture::MockTexture(MockDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockTexture::~MockTexture()
{
}

bool MockTexture::Create()
{
    return true;
}

void* MockTexture::GetHandle() const
{
    return nullptr;
}

uint32_t MockTexture::GetRequiredStagingBufferSize() const
{
    uint32_t size = 0;

    for (uint32_t layer = 0; layer < m_desc.array_size; ++layer)
    {
        for (uint32_t mip = 0; mip < m_desc.mip_levels; ++mip)
        {
            uint32_t height = eastl::max(m_desc.height >> mip, 1u);
            size += GetRowPitch(mip) * height;
        }
    }

    return size;
}

uint32_t MockTexture::GetRowPitch(uint32_t mip_level) const
{
    uint32_t width = eastl::max(m_desc.width >> mip_level, 1u);
    return GetFormatRowPitch(m_desc.format, width);
}

GfxTilingDesc MockTexture::GetTilingDesc() const
{
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc MockTexture::GetTilingDesc(uint32_t subresource) const
{
    return GfxSubresourceTilingDesc();
}

void* MockTexture::GetSharedHandle() const
{
    return nullptr;
}
