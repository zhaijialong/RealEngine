#include "vulkan_texture.h"
#include "vulkan_device.h"
#include "vulkan_heap.h"
#include "utils/log.h"
#include "utils/assert.h"
#include "../gfx.h"

VulkanTexture::VulkanTexture(VulkanDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanTexture::~VulkanTexture()
{
    VulkanDevice* pDevice = (VulkanDevice*)m_pDevice;
    pDevice->CancelDefaultLayoutTransition(this);

    if (!m_bSwapchainImage)
    {
        pDevice->Delete(m_image);
        pDevice->Delete(m_allocation);
    }

    for (size_t i = 0; i < m_renderViews.size(); ++i)
    {
        pDevice->Delete(m_renderViews[i]);
    }
}

bool VulkanTexture::Create()
{
    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();
    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();

    VkImageCreateInfo createInfo = ToVulkanImageCreateInfo(m_desc);

    VkResult result;

    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);

        result = vmaCreateAliasingImage2(allocator, (VmaAllocation)m_desc.heap->GetHandle(), (VkDeviceSize)m_desc.heap_offset, &createInfo, &m_image);
    }
    else
    {
        VmaAllocationCreateInfo allocationInfo = {};
        allocationInfo.usage = ToVmaUsage(m_desc.memory_type);

        if (m_desc.alloc_type == GfxAllocationType::Committed)
        {
            allocationInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkExternalMemoryImageCreateInfo externalMemoryInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
#if RE_PLATFORM_WINDOWS
        externalMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#endif

        if (m_desc.usage & GfxTextureUsageShared)
        {
            createInfo.pNext = &externalMemoryInfo;
            allocationInfo.pool = ((VulkanDevice*)m_pDevice)->GetSharedResourcePool();
        }

        result = vmaCreateImage(allocator, &createInfo, &allocationInfo, &m_image, &m_allocation, nullptr);
    }

    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanTexture] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_IMAGE, m_image, m_name.c_str());

    if (m_allocation)
    {
        vmaSetAllocationName(allocator, m_allocation, m_name.c_str());
    }

    if (m_desc.usage & GfxTextureUsageShared)
    {
        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(allocator, m_allocation, &allocationInfo);

#if RE_PLATFORM_WINDOWS
        VkMemoryGetWin32HandleInfoKHR handleInfo = { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
        handleInfo.memory = allocationInfo.deviceMemory;
        handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

        result = vkGetMemoryWin32HandleKHR(device, &handleInfo, &m_sharedHandle);
        if (result != VK_SUCCESS)
        {
            RE_ERROR("[VulkanTexture] failed to create shared handle for {}", m_name);
            return false;
        }
#endif
    }

    ((VulkanDevice*)m_pDevice)->EnqueueDefaultLayoutTransition(this);

    return true;
}

bool VulkanTexture::Create(VkImage image)
{
    m_image = image;
    m_bSwapchainImage = true;

    SetDebugName((VkDevice)m_pDevice->GetHandle(), VK_OBJECT_TYPE_IMAGE, m_image, m_name.c_str());

    ((VulkanDevice*)m_pDevice)->EnqueueDefaultLayoutTransition(this);

    return true;
}

uint32_t VulkanTexture::GetRequiredStagingBufferSize() const
{
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements((VkDevice)m_pDevice->GetHandle(), m_image, &requirements);
    return (uint32_t)requirements.size;
}

uint32_t VulkanTexture::GetRowPitch(uint32_t mip_level) const
{
    //vkGetImageSubresourceLayout is only valid for linear tiling

    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t width = eastl::max(m_desc.width >> mip_level, min_width);

    return GetFormatRowPitch(m_desc.format, width) * GetFormatBlockHeight(m_desc.format);
}

GfxTilingDesc VulkanTexture::GetTilingDesc() const
{
    //todo
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc VulkanTexture::GetTilingDesc(uint32_t subresource) const
{
    //todo
    return GfxSubresourceTilingDesc();
}

void* VulkanTexture::GetSharedHandle() const
{
#if RE_PLATFORM_WINDOWS
    return m_sharedHandle;
#else
    return nullptr;
#endif
}

VkImageView VulkanTexture::GetRenderView(uint32_t mip_slice, uint32_t array_slice)
{
    RE_ASSERT(m_desc.usage & (GfxTextureUsageRenderTarget | GfxTextureUsageDepthStencil));

    if (m_renderViews.empty())
    {
        m_renderViews.resize(m_desc.mip_levels * m_desc.array_size);
    }

    uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
    if (m_renderViews[index] == VK_NULL_HANDLE)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = m_image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = ToVulkanFormat(m_desc.format, true);
        createInfo.subresourceRange.aspectMask = GetAspectFlags(m_desc.format);
        createInfo.subresourceRange.baseMipLevel = mip_slice;
        createInfo.subresourceRange.baseArrayLayer = array_slice;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.levelCount = 1;

        vkCreateImageView((VkDevice)m_pDevice->GetHandle(), &createInfo, nullptr, &m_renderViews[index]);
    }

    return m_renderViews[index];
}
