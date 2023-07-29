#pragma once

#include "../gfx_swapchain.h"

class VulkanSwapchain : public IGfxSwapchain
{
public:
    virtual void* GetHandle() const override;
    virtual bool Present() override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;
    virtual IGfxTexture* GetBackBuffer() const override;
};