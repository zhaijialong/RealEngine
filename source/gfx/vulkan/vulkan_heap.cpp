#include "vulkan_heap.h"
#include "vulkan_device.h"
#include "utils/log.h"
#include "utils/assert.h"

VulkanHeap::VulkanHeap(VulkanDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanHeap::~VulkanHeap()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_allocation);
}

bool VulkanHeap::Create()
{
    RE_ASSERT(m_desc.size % (64 * 1024) == 0);

    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();

    VkMemoryRequirements requirements = {};
    requirements.size = m_desc.size;
    requirements.alignment = 1; // not used for dedicated allocations
    
    switch (m_desc.memory_type)
    {
    case GfxMemoryType::GpuOnly:
        requirements.memoryTypeBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case GfxMemoryType::CpuOnly:
        requirements.memoryTypeBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    case GfxMemoryType::CpuToGpu:
        requirements.memoryTypeBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        break;
    case GfxMemoryType::GpuToCpu:
        requirements.memoryTypeBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        break;
    default:
        break;
    }

    VmaAllocationCreateInfo createInfo = {};
    createInfo.usage = ToVmaUsage(m_desc.memory_type);
    createInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkResult result = vmaAllocateMemory(allocator, &requirements, &createInfo, &m_allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanHeap] failed to create {}", m_name);
        return false;
    }

    vmaSetAllocationName(allocator, m_allocation, m_name.c_str());

    return true;
}
