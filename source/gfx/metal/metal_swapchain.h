#pragma once

#include "MetalKit/MetalKit.hpp"
#include "../gfx_swapchain.h"

class MetalDevice;
class MetalTexture;

class MetalSwapchain : public IGfxSwapchain
{
public:
    MetalSwapchain(MetalDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name);
    ~MetalSwapchain();

    bool Create();
    MTL::Drawable* GetDrawable();
    
    virtual void* GetHandle() const override { return m_pView; }
    virtual void AcquireNextBackBuffer() override;
    virtual IGfxTexture* GetBackBuffer() const override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;
    
private:
    MTK::View* m_pView = nullptr;
    MetalTexture* m_pTexture = nullptr;
};
