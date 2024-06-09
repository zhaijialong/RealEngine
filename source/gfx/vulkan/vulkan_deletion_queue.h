#pragma once

#include "vulkan_header.h"
#include "EASTL/queue.h"

class VulkanDevice;

class VulkanDeletionQueue
{
public:
    VulkanDeletionQueue(VulkanDevice* device);
    ~VulkanDeletionQueue();

    void Flush(bool force_delete = false);

    template<typename T>
    void Delete(T object, uint64_t frameID);

private:
    VulkanDevice* m_device = nullptr;

    eastl::queue<eastl::pair<VkImage, uint64_t>> m_imageQueue;
    eastl::queue<eastl::pair<VkBuffer, uint64_t>> m_bufferQueue;
    eastl::queue<eastl::pair<VmaAllocation, uint64_t>> m_allocationQueue;
    eastl::queue<eastl::pair<VkImageView, uint64_t>> m_imageViewQueue;
    eastl::queue<eastl::pair<VkSampler, uint64_t>> m_samplerQueue;
    eastl::queue<eastl::pair<VkPipeline, uint64_t>> m_pipelineQueue;
    eastl::queue<eastl::pair<VkShaderModule, uint64_t>> m_shaderQueue;
    eastl::queue<eastl::pair<VkSemaphore, uint64_t>> m_semaphoreQueue;
};
