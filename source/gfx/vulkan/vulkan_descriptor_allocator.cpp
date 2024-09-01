#include "vulkan_descriptor_allocator.h"
#include "vulkan_device.h"
#include "utils/assert.h"

VulkanDescriptorAllocator::VulkanDescriptorAllocator(VulkanDevice* device, uint32_t descriptor_size, uint32_t descriptor_count, VkBufferUsageFlags usage)
{
    m_device = device;
    m_descriptorSize = descriptor_size;
    m_descriptorCount = descriptor_count;

    VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    createInfo.size = descriptor_size * descriptor_count;
    createInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    VmaAllocationInfo allocationInfo;
    vmaCreateBuffer(device->GetVmaAllocator(), &createInfo, &allocationCreateInfo, &m_buffer, &m_allocation, &allocationInfo);

    m_cpuAddress = allocationInfo.pMappedData;

    VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_buffer;
    m_gpuAddress = vkGetBufferDeviceAddress((VkDevice)device->GetHandle(), &info);
}

VulkanDescriptorAllocator::~VulkanDescriptorAllocator()
{
    vmaDestroyBuffer(m_device->GetVmaAllocator(), m_buffer, m_allocation);
}

uint32_t VulkanDescriptorAllocator::Allocate(void** descriptor, size_t* size)
{
    uint32_t index = 0;

    if (!m_freeDescriptors.empty())
    {
        index = m_freeDescriptors.back();
        m_freeDescriptors.pop_back();
    }
    else
    {
        RE_ASSERT(m_allocatedCount < m_descriptorCount);

        index = m_allocatedCount;
        ++m_allocatedCount;
    }

    *descriptor = (char*)m_cpuAddress + m_descriptorSize * index;
    *size = m_descriptorSize;

    return index;
}

void VulkanDescriptorAllocator::Free(uint32_t index)
{
    m_freeDescriptors.push_back(index);
}
