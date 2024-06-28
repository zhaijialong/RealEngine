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
    return m_pDevice->GetAllocationSize(m_desc);
}

uint32_t MockTexture::GetRowPitch(uint32_t mip_level) const
{
    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t width = eastl::max(m_desc.width >> mip_level, min_width);

    return GetFormatRowPitch(m_desc.format, width) * GetFormatBlockHeight(m_desc.format);
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
