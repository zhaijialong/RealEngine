#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include "vulkan_texture.h"
#include "utils/log.h"
#include "utils/assert.h"

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

    VulkanDevice* pDevice = (VulkanDevice*)m_pDevice;
    pDevice->Delete(m_swapchain);
    pDevice->Delete(m_surface);

    for (size_t i = 0; i < m_acquireSemaphores.size(); ++i)
    {
        pDevice->Delete(m_acquireSemaphores[i]);
    }

    for (size_t i = 0; i < m_presentSemaphores.size(); ++i)
    {
        pDevice->Delete(m_presentSemaphores[i]);
    }
}

bool VulkanSwapchain::Create()
{
    if (!CreateSurface() || !CreateSwapchain() || !CreateTextures() || !CreateSemaphores())
    {
        RE_ERROR("[VulkanSwapchain] failed to create {}", m_name);
        return false;
    }

    return true;
}

bool VulkanSwapchain::Resize(uint32_t width, uint32_t height)
{
    if (m_desc.width == width && m_desc.height == height)
    {
        return false;
    }

    m_desc.width = width;
    m_desc.height = height;

    return RecreateSwapchain();
}

void VulkanSwapchain::SetVSyncEnabled(bool value)
{
    //todo
}

void VulkanSwapchain::Present(VkQueue queue)
{
    VkSemaphore waitSemaphore = GetPresentSemaphore();

    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &waitSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_swapchain;
    info.pImageIndices = &m_currentBackBuffer;

    VkResult result = vkQueuePresentKHR(queue, &info);
    
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
    }
}

void VulkanSwapchain::AcquireNextBackBuffer()
{
    m_frameSemaphoreIndex = (m_frameSemaphoreIndex + 1) % m_acquireSemaphores.size();

    VkSemaphore signalSemaphore = GetAcquireSemaphore();

    VkResult result = vkAcquireNextImageKHR((VkDevice)m_pDevice->GetHandle(), m_swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &m_currentBackBuffer);
    
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();

        result = vkAcquireNextImageKHR((VkDevice)m_pDevice->GetHandle(), m_swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &m_currentBackBuffer);
        RE_ASSERT(result == VK_SUCCESS);
    }
}

IGfxTexture* VulkanSwapchain::GetBackBuffer() const
{
    return m_backBuffers[m_currentBackBuffer];
}

VkSemaphore VulkanSwapchain::GetAcquireSemaphore()
{
    return m_acquireSemaphores[m_frameSemaphoreIndex];
}

VkSemaphore VulkanSwapchain::GetPresentSemaphore()
{
    return m_presentSemaphores[m_frameSemaphoreIndex];
}

bool VulkanSwapchain::CreateSurface()
{
    VkInstance instance = ((VulkanDevice*)m_pDevice)->GetInstance();
    VkDevice device = (VkDevice)m_pDevice->GetHandle();

    VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = (HWND)m_desc.window_handle;

    VkResult result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &m_surface);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_SURFACE_KHR, m_surface, m_name.c_str());

    return true;
}

bool VulkanSwapchain::CreateSwapchain()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();

    VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = m_surface;
    createInfo.minImageCount = m_desc.backbuffer_count;
    createInfo.imageFormat = ToVulkanFormat(m_desc.backbuffer_format);
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = m_desc.width;
    createInfo.imageExtent.height = m_desc.height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // todo
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_swapchain;

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_swapchain, m_name.c_str());

    return true;
}

bool VulkanSwapchain::CreateTextures()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();

    GfxTextureDesc textureDesc;
    textureDesc.width = m_desc.width;
    textureDesc.height = m_desc.height;
    textureDesc.format = m_desc.backbuffer_format;
    textureDesc.usage = GfxTextureUsageRenderTarget;

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr);
    eastl::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, images.data());

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        eastl::string name = fmt::format("{} texture {}", m_name, i).c_str();

        SetDebugName(device, VK_OBJECT_TYPE_IMAGE, images[i], name.c_str());

        VulkanTexture* texture = new VulkanTexture((VulkanDevice*)m_pDevice, textureDesc, name);
        texture->m_image = images[i];
        texture->m_bSwapchainImage = true;

        m_backBuffers.push_back(texture);
    }

    return true;
}

bool VulkanSwapchain::CreateSemaphores()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
    {
        VkSemaphore semaphore;
        VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS)
        {
            return false;
        }

        SetDebugName(device, VK_OBJECT_TYPE_SEMAPHORE, semaphore, fmt::format("{} acquire semaphore {}", m_name, i).c_str());

        m_acquireSemaphores.push_back(semaphore);
    }

    for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
    {
        VkSemaphore semaphore;
        VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS)
        {
            return false;
        }

        SetDebugName(device, VK_OBJECT_TYPE_SEMAPHORE, semaphore, fmt::format("{} present semaphore {}", m_name, i).c_str());

        m_presentSemaphores.push_back(semaphore);
    }

    return true;
}

bool VulkanSwapchain::RecreateSwapchain()
{
    //todo
    return false;
}
