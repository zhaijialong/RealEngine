#pragma once

#include "../gfx_texture.h"

class MetalTexture : public IGfxTexture
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const override;
    virtual void* GetSharedHandle() const override;
};
