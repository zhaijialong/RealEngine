#pragma once

#include "../gfx_texture.h"

class VulkanTexture : public IGfxTexture
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource) const override;
};
