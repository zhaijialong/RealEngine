#pragma once

#include "../gfx_texture.h"

class WebGPUDevice;

class WebGPUTexture : public IGfxTexture
{
public:
    WebGPUTexture(WebGPUDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name);
    ~WebGPUTexture();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const override;
    virtual void* GetSharedHandle() const override;
};
