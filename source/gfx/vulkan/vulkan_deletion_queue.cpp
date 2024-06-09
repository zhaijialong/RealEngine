#include "vulkan_deletion_queue.h"
#include "vulkan_device.h"

VulkanDeletionQueue::VulkanDeletionQueue(VulkanDevice* device)
{
    m_device = device;
}

VulkanDeletionQueue::~VulkanDeletionQueue()
{
    Flush(true);
}

void VulkanDeletionQueue::Flush(bool force_delete)
{
    uint64_t frameID = m_device->GetFrameID();
    VkDevice device = m_device->GetDevice();
    VmaAllocator allocator = m_device->GetVmaAllocator();

#define ITERATE_QUEUE(queue, deletion_func) \
    while (!queue.empty()) \
    { \
        auto item = queue.front(); \
        if (!force_delete && item.second + GFX_MAX_INFLIGHT_FRAMES > frameID) \
        { \
            break; \
        } \
        deletion_func(device, item.first, nullptr); \
        queue.pop(); \
    }

    ITERATE_QUEUE(m_imageQueue, vkDestroyImage);
    ITERATE_QUEUE(m_bufferQueue, vkDestroyBuffer);
    ITERATE_QUEUE(m_imageViewQueue, vkDestroyImageView);
    ITERATE_QUEUE(m_samplerQueue, vkDestroySampler);
    ITERATE_QUEUE(m_pipelineQueue, vkDestroyPipeline);
    ITERATE_QUEUE(m_shaderQueue, vkDestroyShaderModule);
    ITERATE_QUEUE(m_semaphoreQueue, vkDestroySemaphore);

    while (!m_allocationQueue.empty())
    {
        auto item = m_allocationQueue.front();
        if (!force_delete && item.second + GFX_MAX_INFLIGHT_FRAMES > frameID)
        {
            break;
        }

        vmaFreeMemory(allocator, item.first);
        m_allocationQueue.pop();
    }
}

template<>
void VulkanDeletionQueue::Delete(VkImage object, uint64_t frameID)
{
    m_imageQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkBuffer object, uint64_t frameID)
{
    m_bufferQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VmaAllocation object, uint64_t frameID)
{
    m_allocationQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkImageView object, uint64_t frameID)
{
    m_imageViewQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkSampler object, uint64_t frameID)
{
    m_samplerQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkPipeline object, uint64_t frameID)
{
    m_pipelineQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkShaderModule object, uint64_t frameID)
{
    m_shaderQueue.push(eastl::make_pair(object, frameID));
}

template<>
void VulkanDeletionQueue::Delete(VkSemaphore object, uint64_t frameID)
{
    m_semaphoreQueue.push(eastl::make_pair(object, frameID));
}