#include "vulkan_constant_buffer_allocator.h"
#include "vulkan_device.h"
#include "utils/assert.h"
#include "utils/math.h"

VulkanConstantBufferAllocator::VulkanConstantBufferAllocator(VulkanDevice* device, uint32_t buffer_size)
{
    m_device = device;
    m_bufferSize = buffer_size;

    VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    createInfo.size = buffer_size;
    createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

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

VulkanConstantBufferAllocator::~VulkanConstantBufferAllocator()
{
    vmaDestroyBuffer(m_device->GetVmaAllocator(), m_buffer, m_allocation);
}

void VulkanConstantBufferAllocator::Allocate(uint32_t size, void** cpu_address, VkDeviceAddress* gpu_address)
{
    RE_ASSERT(m_allocatedSize + size <= m_bufferSize);

    *cpu_address = (char*)m_cpuAddress + m_allocatedSize;
    *gpu_address = m_gpuAddress + m_allocatedSize;

    m_allocatedSize += RoundUpPow2(size, 256);
}

void VulkanConstantBufferAllocator::Reset()
{
    m_allocatedSize = 0;
}
