#pragma once

#include "vulkan_header.h"
#include "../gfx_texture.h"

class VulkanDevice;

class VulkanTexture : public IGfxTexture
{
public:
    VulkanTexture(VulkanDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name);
    ~VulkanTexture();

    bool Create();
    bool Create(VkImage image); // used only by the swapchain
    bool IsSwapchainTexture() const { return m_bSwapchainImage; }
    VkImageView GetRenderView(uint32_t mip_slice, uint32_t array_slice);

    virtual void* GetHandle() const override { return m_image; }
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mip_level = 0) const override;
    virtual GfxTilingDesc GetTilingDesc() const override;
    virtual GfxSubresourceTilingDesc GetTilingDesc(uint32_t subresource = 0) const override;
    virtual void* GetSharedHandle() const override;

private:
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    bool m_bSwapchainImage = false;

    eastl::vector<VkImageView> m_renderViews;
};
