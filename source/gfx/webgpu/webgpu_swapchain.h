#pragma once

#include "../gfx_swapchain.h"

class WebGPUDevice;

class WebGPUSwapchain : public IGfxSwapchain
{
public:
    WebGPUSwapchain(WebGPUDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name);
    ~WebGPUSwapchain();

    bool Create();
    void Present();

    virtual void* GetHandle() const override;
    virtual void AcquireNextBackBuffer() override;
    virtual IGfxTexture* GetBackBuffer() const override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;

private:
    bool CreateTextures();

private:
    int32_t m_currentBackBuffer = -1;
    eastl::vector<IGfxTexture*> m_backBuffers;
};