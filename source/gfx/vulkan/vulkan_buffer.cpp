#include "vulkan_buffer.h"
#include "vulkan_device.h"
#include "vulkan_heap.h"
#include "utils/log.h"
#include "utils/assert.h"

VulkanBuffer::VulkanBuffer(VulkanDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanBuffer::~VulkanBuffer()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_buffer);
    ((VulkanDevice*)m_pDevice)->Delete(m_allocation);
}

bool VulkanBuffer::Create()
{
    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();
    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();

    VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    createInfo.size = m_desc.size;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    if (m_desc.usage & GfxBufferUsageConstantBuffer)
    {
        createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if (m_desc.usage & (GfxBufferUsageStructuredBuffer | GfxBufferUsageRawBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if (m_desc.usage & GfxBufferUsageTypedBuffer)
    {
        if (m_desc.usage & GfxBufferUsageUnorderedAccess)
        {
            createInfo.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        }
        else
        {
            createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        }
    }

    VkResult result;

    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);
        RE_ASSERT(m_desc.size + m_desc.heap_offset <= m_desc.heap->GetDesc().size);

        result = vmaCreateAliasingBuffer2(allocator, (VmaAllocation)m_desc.heap->GetHandle(), (VkDeviceSize)m_desc.heap_offset, &createInfo, &m_buffer);
    }
    else
    {
        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = ToVmaUsage(m_desc.memory_type);

        if (m_desc.alloc_type == GfxAllocationType::Committed)
        {
            allocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        if (m_desc.memory_type != GfxMemoryType::GpuOnly)
        {
            allocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VmaAllocationInfo allocationInfo;
        result = vmaCreateBuffer(allocator, &createInfo, &allocationCreateInfo, &m_buffer, &m_allocation, &allocationInfo);

        m_pCpuAddress = allocationInfo.pMappedData;
    }

    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanBuffer] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_BUFFER, m_buffer, m_name.c_str());

    if (m_allocation)
    {
        vmaSetAllocationName(allocator, m_allocation, m_name.c_str());
    }

    return true;
}

void* VulkanBuffer::GetCpuAddress()
{
    return m_pCpuAddress;
}

uint64_t VulkanBuffer::GetGpuAddress()
{
    VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_buffer;

    return vkGetBufferDeviceAddress((VkDevice)m_pDevice->GetHandle(), &info);
}

uint32_t VulkanBuffer::GetRequiredStagingBufferSize() const
{
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements((VkDevice)m_pDevice->GetHandle(), m_buffer, &requirements);
    return requirements.size;
}
