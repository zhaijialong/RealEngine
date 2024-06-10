#pragma once

#include "vulkan_header.h"
#include "../gfx_fence.h"

class VulkanDevice;

class VulkanFence : public IGfxFence
{
public:
    VulkanFence(VulkanDevice* pDevice, const eastl::string& name);
    ~VulkanFence();

    bool Create();

    virtual void* GetHandle() const override { return m_semaphore; }
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};