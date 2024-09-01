#pragma once

#include "vulkan_header.h"
#include "../gfx_descriptor.h"
#include "../gfx_buffer.h"

class VulkanDevice;

class VulkanShaderResourceView : public IGfxDescriptor
{
public:
    VulkanShaderResourceView(VulkanDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name);
    ~VulkanShaderResourceView();

    bool Create();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
    VkImageView m_imageView = VK_NULL_HANDLE;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class VulkanUnorderedAccessView : public IGfxDescriptor
{
public:
    VulkanUnorderedAccessView(VulkanDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name);
    ~VulkanUnorderedAccessView();

    bool Create();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
    VkImageView m_imageView = VK_NULL_HANDLE;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class VulkanConstantBufferView : public IGfxDescriptor
{
public:
    VulkanConstantBufferView(VulkanDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name);
    ~VulkanConstantBufferView();

    bool Create();

    virtual void* GetHandle() const override { return m_pBuffer->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class VulkanSampler : public IGfxDescriptor
{
public:
    VulkanSampler(VulkanDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name);
    ~VulkanSampler();

    bool Create();

    virtual void* GetHandle() const override { return m_sampler; }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    GfxSamplerDesc m_desc;
    VkSampler m_sampler = VK_NULL_HANDLE;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};