#pragma once

#include "vulkan_header.h"
#include "../gfx_swapchain.h"

class VulkanDevice;

class VulkanSwapchain : public IGfxSwapchain
{
public:
    VulkanSwapchain(VulkanDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name);
    ~VulkanSwapchain();

    bool Create();

    virtual void* GetHandle() const override { return m_swapchain; }
    virtual bool Present() override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;
    virtual IGfxTexture* GetBackBuffer() const override;

private:
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    bool m_bEnableVsync = true;
    eastl::vector<IGfxTexture*> m_backBuffers;
};