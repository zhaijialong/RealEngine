#include "vulkan_texture.h"
#include "vulkan_device.h"
#include "vulkan_heap.h"
#include "utils/log.h"
#include "utils/assert.h"

VulkanTexture::VulkanTexture(VulkanDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanTexture::~VulkanTexture()
{
    VulkanDevice* pDevice = (VulkanDevice*)m_pDevice;

    if (!m_bSwapchainImage)
    {
        pDevice->Delete(m_image);
        pDevice->Delete(m_allocation);
    }

    for (size_t i = 0; i < m_rtv.size(); ++i)
    {
        pDevice->Delete(m_rtv[i]);
    }

    for (size_t i = 0; i < m_dsv.size(); ++i)
    {
        pDevice->Delete(m_dsv[i]);
    }
}

bool VulkanTexture::Create()
{
    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();
    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();

    VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    createInfo.imageType = m_desc.type == GfxTextureType::Texture3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    createInfo.format = ToVulkanFormat(m_desc.format);
    createInfo.extent.width = m_desc.width;
    createInfo.extent.height = m_desc.height;
    createInfo.extent.depth = m_desc.depth;
    createInfo.mipLevels = m_desc.mip_levels;
    createInfo.arrayLayers = m_desc.array_size;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (m_desc.usage & GfxTextureUsageRenderTarget)
    {
        createInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (m_desc.usage & GfxTextureUsageDepthStencil)
    {
        createInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (m_desc.usage & GfxTextureUsageUnorderedAccess)
    {
        createInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (m_desc.type == GfxTextureType::TextureCube || m_desc.type == GfxTextureType::TextureCubeArray)
    {
        createInfo.arrayLayers *= 6; // cube faces count as array layers in Vulkan
        createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    if (m_desc.alloc_type == GfxAllocationType::Sparse)
    {
        createInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    }

    VkResult result;

    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);

        result = vmaCreateAliasingImage2(allocator, (VmaAllocation)m_desc.heap->GetHandle(), (VkDeviceSize)m_desc.heap_offset, &createInfo, &m_image);
    }
    else
    {
        VmaAllocationCreateInfo allocationInfo = {};
        allocationInfo.usage = ToVmaUsage(m_desc.memory_type);

        if (m_desc.alloc_type == GfxAllocationType::Committed)
        {
            allocationInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        result = vmaCreateImage(allocator, &createInfo, &allocationInfo, &m_image, &m_allocation, nullptr);
    }

    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanTexture] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_IMAGE, m_image, m_name.c_str());

    if (m_allocation)
    {
        vmaSetAllocationName(allocator, m_allocation, m_name.c_str());
    }

    return true;
}

uint32_t VulkanTexture::GetRequiredStagingBufferSize() const
{
    //todo
    return 0;
}

uint32_t VulkanTexture::GetRowPitch(uint32_t mip_level) const
{
    //todo
    return 0;
}

GfxTilingDesc VulkanTexture::GetTilingDesc() const
{
    //todo
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc VulkanTexture::GetTilingDesc(uint32_t subresource) const
{
    //todo
    return GfxSubresourceTilingDesc();
}

void* VulkanTexture::GetSharedHandle() const
{
    //todo
    return nullptr;
}
