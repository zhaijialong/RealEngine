#pragma once

#include "i_gfx_resource.h"

class IGfxTexture;

class IGfxSwapchain : public IGfxResource
{
public:
    virtual ~IGfxSwapchain() {}

    virtual bool Present() = 0;
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
    virtual void SetVSyncEnabled(bool value) = 0;
    virtual IGfxTexture* GetBackBuffer() const = 0;

    const GfxSwapchainDesc& GetDesc() const { return m_desc; }

protected:
    GfxSwapchainDesc m_desc = {};
};