#pragma once

#include "vulkan_header.h"

class VulkanDevice;

class VulkanConstantBufferAllocator
{
public:
    VulkanConstantBufferAllocator(VulkanDevice* device, uint32_t buffer_size);
    ~VulkanConstantBufferAllocator();
    
    void Allocate(uint32_t size, void** cpu_address, VkDeviceAddress* gpu_address);
    void Reset();
    VkDeviceAddress GetGpuAddress() const { return m_gpuAddress; }

private:
    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_gpuAddress = 0;
    void* m_cpuAddress = nullptr;
    uint32_t m_bufferSize = 0;
    uint32_t m_allocatedSize = 0;
};