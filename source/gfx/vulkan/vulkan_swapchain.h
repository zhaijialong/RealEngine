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
    void Present(VkQueue queue);
    VkSemaphore GetAcquireSemaphore();
    VkSemaphore GetPresentSemaphore();

    virtual void* GetHandle() const override { return m_swapchain; }
    virtual void AcquireNextBackBuffer() override;
    virtual IGfxTexture* GetBackBuffer() const override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVSyncEnabled(bool value) override;

private:
    bool CreateSurface();
    bool CreateSwapchain();
    bool CreateTextures();
    bool CreateSemaphores();

    bool RecreateSwapchain();

private:
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    bool m_bEnableVsync = true;
    bool m_bMailboxSupported = false;

    uint32_t m_currentBackBuffer = 0;
    eastl::vector<IGfxTexture*> m_backBuffers;

    int32_t m_frameSemaphoreIndex = -1;
    eastl::vector<VkSemaphore> m_acquireSemaphores;
    eastl::vector<VkSemaphore> m_presentSemaphores;
};