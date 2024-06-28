#pragma once

#include "../gfx_swapchain.h"

class MetalDevice;

class MetalSwapchain : public IGfxSwapchain
{
public:
    MetalSwapchain(MetalDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name);
    ~MetalSwapchain();

    bool Create();
    void Present();
    
    virtual void* GetHandle() const override;
    virtual void AcquireNextBackBuffer() override;
    virtual IGfxTexture* GetBackBuffer() const override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;
};
