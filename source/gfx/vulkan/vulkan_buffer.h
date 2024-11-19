#pragma once

#include "vulkan_header.h"
#include "../gfx_buffer.h"

class VulkanDevice;

class VulkanBuffer : public IGfxBuffer
{
public:
    VulkanBuffer(VulkanDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name);
    ~VulkanBuffer();

    bool Create();

    virtual void* GetHandle() const override { return m_buffer; }
    virtual void* GetCpuAddress() override;
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual void* GetSharedHandle() const override;

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    void* m_pCpuAddress = nullptr;

#if RE_PLATFORM_WINDOWS
    HANDLE m_sharedHandle = 0;
#endif
};
