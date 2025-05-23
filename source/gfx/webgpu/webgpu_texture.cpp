#include "webgpu_texture.h"
#include "webgpu_device.h"
#include "../gfx.h"

WebGPUTexture::WebGPUTexture(WebGPUDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPUTexture::~WebGPUTexture()
{
}

bool WebGPUTexture::Create()
{
    return true;
}

void* WebGPUTexture::GetHandle() const
{
    return nullptr;
}

uint32_t WebGPUTexture::GetRequiredStagingBufferSize() const
{
    return m_pDevice->GetAllocationSize(m_desc);
}

uint32_t WebGPUTexture::GetRowPitch(uint32_t mip_level) const
{
    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t width = eastl::max(m_desc.width >> mip_level, min_width);

    return GetFormatRowPitch(m_desc.format, width) * GetFormatBlockHeight(m_desc.format);
}

GfxTilingDesc WebGPUTexture::GetTilingDesc() const
{
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc WebGPUTexture::GetTilingDesc(uint32_t subresource) const
{
    return GfxSubresourceTilingDesc();
}

void* WebGPUTexture::GetSharedHandle() const
{
    return nullptr;
}
