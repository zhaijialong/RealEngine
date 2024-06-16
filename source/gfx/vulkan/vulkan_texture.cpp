#include "vulkan_texture.h"
#include "vulkan_device.h"
#include "vulkan_heap.h"
#include "utils/log.h"
#include "utils/assert.h"

VulkanTexture::VulkanTexture(VulkanDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanTexture::~VulkanTexture()
{
    VulkanDevice* pDevice = (VulkanDevice*)m_pDevice;

    if (!m_bSwapchainImage)
    {
        pDevice->Delete(m_image);
        pDevice->Delete(m_allocation);
    }

    for (size_t i = 0; i < m_rtv.size(); ++i)
    {
        pDevice->Delete(m_rtv[i]);
    }

    for (size_t i = 0; i < m_dsv.size(); ++i)
    {
        pDevice->Delete(m_dsv[i]);
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

    return true;
}

bool VulkanTexture::Create(VkImage image)
{
    m_image = image;
    m_bSwapchainImage = true;

    SetDebugName((VkDevice)m_pDevice->GetHandle(), VK_OBJECT_TYPE_IMAGE, m_image, m_name.c_str());

    return true;
}

uint32_t VulkanTexture::GetRequiredStagingBufferSize() const
{
    //todo
    return 0;
}

uint32_t VulkanTexture::GetRowPitch(uint32_t mip_level) const
{
    //todo
    return 0;
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
    //todo
    return nullptr;
}
