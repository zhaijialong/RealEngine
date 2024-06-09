#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include "vulkan_texture.h"

VulkanSwapchain::VulkanSwapchain(VulkanDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanSwapchain::~VulkanSwapchain()
{
    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();
}

bool VulkanSwapchain::Create()
{
    return false;
}

bool VulkanSwapchain::Present()
{
    return false;
}

bool VulkanSwapchain::Resize(uint32_t width, uint32_t height)
{
    return false;
}

void VulkanSwapchain::SetVSyncEnabled(bool value)
{
}

IGfxTexture* VulkanSwapchain::GetBackBuffer() const
{
    return nullptr;
}
