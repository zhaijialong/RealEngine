#pragma once

#include "gfx_resource.h"

class IGfxTexture : public IGfxResource
{
public:
    const GfxTextureDesc& GetDesc() const { return m_desc; }

    virtual bool IsTexture() const { return true; }
    virtual uint32_t GetRequiredStagingBufferSize() const = 0;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const = 0;
    virtual GfxTilingDesc GetTilingDesc() const = 0;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const = 0;
    virtual void* GetSharedHandle() const = 0;

protected:
    GfxTextureDesc m_desc = {};
};
