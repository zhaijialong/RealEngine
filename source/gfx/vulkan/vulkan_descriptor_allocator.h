#pragma once

#include "vulkan_header.h"

class VulkanDevice;

class VulkanDescriptorAllocator
{
public:
    VulkanDescriptorAllocator(VulkanDevice* device, uint32_t descriptor_size, uint32_t descriptor_count, VkBufferUsageFlags usage);
    ~VulkanDescriptorAllocator();

    uint32_t Allocate(void** descriptor);
    void Free(uint32_t index);

    VkDeviceAddress GetGpuAddress() const { return m_gpuAddress; }

private:
    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_gpuAddress = 0;
    void* m_cpuAddress = nullptr;
    uint32_t m_descriptorSize = 0;
    uint32_t m_descriptorCount = 0;

    uint32_t m_allocatedCount = 0;
    eastl::vector<uint32_t> m_freeDescriptors;
};