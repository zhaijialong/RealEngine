#include "metal_texture.h"
#include "metal_device.h"
#include "../gfx.h"

MetalTexture::MetalTexture(MetalDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalTexture::~MetalTexture()
{
    m_pTexture->release();
}

bool MetalTexture::Create()
{
    return true;
}

uint32_t MetalTexture::GetRequiredStagingBufferSize() const
{
    return m_pDevice->GetAllocationSize(m_desc);
}

uint32_t MetalTexture::GetRowPitch(uint32_t mip_level) const
{
    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t width = eastl::max(m_desc.width >> mip_level, min_width);

    return GetFormatRowPitch(m_desc.format, width) * GetFormatBlockHeight(m_desc.format);
}

GfxTilingDesc MetalTexture::GetTilingDesc() const
{
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc MetalTexture::GetTilingDesc(uint32_t subresource) const
{
    return GfxSubresourceTilingDesc();
}

void* MetalTexture::GetSharedHandle() const
{
    return nullptr;
}
