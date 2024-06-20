#pragma once

#include "vulkan_header.h"
#include "../gfx_heap.h"

class VulkanDevice;

class VulkanHeap : public IGfxHeap
{
public:
    VulkanHeap(VulkanDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name);
    ~VulkanHeap();

    bool Create();

    virtual void* GetHandle() const override { return m_allocation; }

private:
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};