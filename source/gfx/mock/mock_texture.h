#pragma once

#include "../gfx_texture.h"

class MockDevice;

class MockTexture : public IGfxTexture
{
public:
    MockTexture(MockDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name);
    ~MockTexture();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const override;
    virtual void* GetSharedHandle() const override;
};
