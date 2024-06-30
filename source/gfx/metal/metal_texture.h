#pragma once

#include "Metal/MTLTexture.hpp"
#include "../gfx_texture.h"

class MetalDevice;

class MetalTexture : public IGfxTexture
{
public:
    MetalTexture(MetalDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name);
    ~MetalTexture();

    bool Create();
    void SetSwapchainTexture(MTL::Texture* texture);
    
    virtual void* GetHandle() const override { return m_pTexture; }
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const override;
    virtual void* GetSharedHandle() const override;
    
private:
    MTL::Texture* m_pTexture = nullptr;
    bool m_bSwapchainTexture = false;
};
