#include "vulkan_descriptor.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"
#include "utils/log.h"

VulkanShaderResourceView::VulkanShaderResourceView(VulkanDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

VulkanShaderResourceView::~VulkanShaderResourceView()
{
}

bool VulkanShaderResourceView::Create()
{
    return true;
}

VulkanUnorderedAccessView::VulkanUnorderedAccessView(VulkanDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

VulkanUnorderedAccessView::~VulkanUnorderedAccessView()
{
}

bool VulkanUnorderedAccessView::Create()
{
    return true;
}

VulkanConstantBufferView::VulkanConstantBufferView(VulkanDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pBuffer = buffer;
    m_desc = desc;
}

VulkanConstantBufferView::~VulkanConstantBufferView()
{
}

bool VulkanConstantBufferView::Create()
{
    return true;
}

VulkanSampler::VulkanSampler(VulkanDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
}

VulkanSampler::~VulkanSampler()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_sampler);
    ((VulkanDevice*)m_pDevice)->FreeSamplerDescriptor(m_heapIndex);
}

bool VulkanSampler::Create()
{
    VkSamplerReductionModeCreateInfo reductionMode = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
    reductionMode.reductionMode = ToVkSamplerReductionMode(m_desc.reduction_mode);

    VkSamplerCreateInfo createInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    createInfo.pNext = &reductionMode;
    createInfo.magFilter = ToVkFilter(m_desc.mag_filter);
    createInfo.minFilter = ToVkFilter(m_desc.min_filter);
    createInfo.mipmapMode = ToVkSamplerMipmapMode(m_desc.mip_filter);
    createInfo.addressModeU = ToVkSamplerAddressMode(m_desc.address_u);
    createInfo.addressModeV = ToVkSamplerAddressMode(m_desc.address_v);
    createInfo.addressModeW = ToVkSamplerAddressMode(m_desc.address_w);
    createInfo.mipLodBias = m_desc.mip_bias;
    createInfo.anisotropyEnable = m_desc.enable_anisotropy;
    createInfo.maxAnisotropy = m_desc.max_anisotropy;
    createInfo.compareEnable = m_desc.reduction_mode == GfxSamplerReductionMode::Compare;
    createInfo.compareOp = ToVkCompareOp(m_desc.compare_func);
    createInfo.minLod = m_desc.min_lod;
    createInfo.maxLod = m_desc.max_lod;

    if (m_desc.border_color[0] == 0.0f && m_desc.border_color[1] == 0.0f && m_desc.border_color[2] == 0.0f && m_desc.border_color[2] == 0.0f)
    {
        createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
    else if (m_desc.border_color[0] == 0.0f && m_desc.border_color[1] == 0.0f && m_desc.border_color[2] == 0.0f && m_desc.border_color[2] == 1.0f)
    {
        createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
    else if (m_desc.border_color[0] == 1.0f && m_desc.border_color[1] == 1.0f && m_desc.border_color[2] == 1.0f && m_desc.border_color[2] == 1.0f)
    {
        createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    else
    {
        RE_ASSERT(false); //unsupported border color
    }

    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    VkResult result = vkCreateSampler(device, &createInfo, nullptr, &m_sampler);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanSampler] failed to create : {}", m_name);
        return false;
    }

    void* pDescriptor = nullptr;
    size_t size = 0;
    m_heapIndex = ((VulkanDevice*)m_pDevice)->AllocateSamplerDescriptor(&pDescriptor, &size);

    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorInfo.data.pSampler = &m_sampler;
    vkGetDescriptorEXT(device, &descriptorInfo, size, pDescriptor);

    return true;
}
