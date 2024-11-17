#include "vulkan_descriptor.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"
#include "vulkan_rt_tlas.h"
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
    VulkanDevice* device = (VulkanDevice*)m_pDevice;
    device->Delete(m_imageView);
    device->FreeResourceDescriptor(m_heapIndex);
}

bool VulkanShaderResourceView::Create()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& descriptorBufferProperties = ((VulkanDevice*)m_pDevice)->GetDescriptorBufferProperties();

    size_t descriptorSize = 0;
    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkImageViewUsageCreateInfo imageViewUsageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    imageViewUsageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    if (m_pResource && m_pResource->IsTexture())
    {
        imageViewCreateInfo.pNext = &imageViewUsageInfo;
        imageViewCreateInfo.image = (VkImage)m_pResource->GetHandle();
        imageViewCreateInfo.format = ToVulkanFormat(m_desc.format, true);
        imageViewCreateInfo.subresourceRange.aspectMask = GetAspectFlags(m_desc.format);
        imageViewCreateInfo.subresourceRange.baseMipLevel = m_desc.texture.mip_slice;
        imageViewCreateInfo.subresourceRange.levelCount = m_desc.texture.mip_levels;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = m_desc.texture.array_slice;
        imageViewCreateInfo.subresourceRange.layerCount = m_desc.texture.array_size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorInfo.data.pSampledImage = &imageInfo;
        descriptorSize = descriptorBufferProperties.sampledImageDescriptorSize;
    }

    switch (m_desc.type)
    {
    case GfxShaderResourceViewType::Texture2D:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxShaderResourceViewType::Texture2DArray:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxShaderResourceViewType::Texture3D:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxShaderResourceViewType::TextureCube:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxShaderResourceViewType::TextureCubeArray:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxShaderResourceViewType::StructuredBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageStructuredBuffer);
        RE_ASSERT(m_desc.format == GfxFormat::Unknown);
        RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
        RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustStorageBufferDescriptorSize;
        break;
    }
    case GfxShaderResourceViewType::TypedBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageTypedBuffer);
        RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
        RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;
        bufferInfo.format = ToVulkanFormat(m_desc.format);

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descriptorInfo.data.pUniformTexelBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustUniformTexelBufferDescriptorSize;
        break;
    }
    case GfxShaderResourceViewType::RawBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageRawBuffer);
        RE_ASSERT(bufferDesc.stride % 4 == 0);
        RE_ASSERT(m_desc.buffer.offset % 4 == 0);
        RE_ASSERT(m_desc.buffer.size % 4 == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustStorageBufferDescriptorSize;
        break;
    }
    case GfxShaderResourceViewType::RayTracingTLAS:
    {
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        descriptorInfo.data.accelerationStructure = ((VulkanRayTracingTLAS*)m_pResource)->GetGpuAddress();

        descriptorSize = descriptorBufferProperties.accelerationStructureDescriptorSize;
        break;
    }
    default:
        break;
    }

    void* pDescriptor = nullptr;
    m_heapIndex = ((VulkanDevice*)m_pDevice)->AllocateResourceDescriptor(&pDescriptor);

    vkGetDescriptorEXT(device, &descriptorInfo, descriptorSize, pDescriptor);

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
    VulkanDevice* device = (VulkanDevice*)m_pDevice;
    device->Delete(m_imageView);
    device->FreeResourceDescriptor(m_heapIndex);
}

bool VulkanUnorderedAccessView::Create()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& descriptorBufferProperties = ((VulkanDevice*)m_pDevice)->GetDescriptorBufferProperties();

    size_t descriptorSize = 0;
    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkImageViewUsageCreateInfo imageViewUsageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    imageViewUsageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;

    VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    if (m_pResource && m_pResource->IsTexture())
    {
        const GfxTextureDesc& textureDesc = ((IGfxTexture*)m_pResource)->GetDesc();
        RE_ASSERT(textureDesc.usage & GfxTextureUsageUnorderedAccess);

        imageViewCreateInfo.pNext = &imageViewUsageInfo;
        imageViewCreateInfo.image = (VkImage)m_pResource->GetHandle();
        imageViewCreateInfo.format = ToVulkanFormat(m_desc.format);
        imageViewCreateInfo.subresourceRange.aspectMask = GetAspectFlags(m_desc.format);
        imageViewCreateInfo.subresourceRange.baseMipLevel = m_desc.texture.mip_slice;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = m_desc.texture.array_slice;
        imageViewCreateInfo.subresourceRange.layerCount = m_desc.texture.array_size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorInfo.data.pStorageImage = &imageInfo;
        descriptorSize = descriptorBufferProperties.storageImageDescriptorSize;
    }

    switch (m_desc.type)
    {
    case GfxUnorderedAccessViewType::Texture2D:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxUnorderedAccessViewType::Texture2DArray:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxUnorderedAccessViewType::Texture3D:
    {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView);

        imageInfo.imageView = m_imageView;
        break;
    }
    case GfxUnorderedAccessViewType::StructuredBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageStructuredBuffer);
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
        RE_ASSERT(m_desc.format == GfxFormat::Unknown);
        RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
        RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustStorageBufferDescriptorSize;
        break;
    }
    case GfxUnorderedAccessViewType::TypedBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageTypedBuffer);
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
        RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
        RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;
        bufferInfo.format = ToVulkanFormat(m_desc.format);

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        descriptorInfo.data.pUniformTexelBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustUniformTexelBufferDescriptorSize;
        break;
    }
    case GfxUnorderedAccessViewType::RawBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageRawBuffer);
        RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
        RE_ASSERT(bufferDesc.stride % 4 == 0);
        RE_ASSERT(m_desc.buffer.offset % 4 == 0);
        RE_ASSERT(m_desc.buffer.size % 4 == 0);

        bufferInfo.address = ((IGfxBuffer*)m_pResource)->GetGpuAddress() + m_desc.buffer.offset;
        bufferInfo.range = m_desc.buffer.size;

        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        descriptorSize = descriptorBufferProperties.robustStorageBufferDescriptorSize;
        break;
    }
    default:
        break;
    }

    void* pDescriptor = nullptr;
    m_heapIndex = ((VulkanDevice*)m_pDevice)->AllocateResourceDescriptor(&pDescriptor);

    vkGetDescriptorEXT(device, &descriptorInfo, descriptorSize, pDescriptor);

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
    ((VulkanDevice*)m_pDevice)->FreeResourceDescriptor(m_heapIndex);
}

bool VulkanConstantBufferView::Create()
{
    VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
    bufferInfo.address = m_pBuffer->GetGpuAddress() + m_desc.offset;
    bufferInfo.range = m_desc.size;

    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorInfo.data.pUniformBuffer = &bufferInfo;

    void* pDescriptor = nullptr;
    m_heapIndex = ((VulkanDevice*)m_pDevice)->AllocateResourceDescriptor(&pDescriptor);

    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    size_t size = ((VulkanDevice*)m_pDevice)->GetDescriptorBufferProperties().robustUniformBufferDescriptorSize;
    vkGetDescriptorEXT(device, &descriptorInfo, size, pDescriptor);

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
    m_heapIndex = ((VulkanDevice*)m_pDevice)->AllocateSamplerDescriptor(&pDescriptor);

    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorInfo.data.pSampler = &m_sampler;

    size_t size = ((VulkanDevice*)m_pDevice)->GetDescriptorBufferProperties().samplerDescriptorSize;
    vkGetDescriptorEXT(device, &descriptorInfo, size, pDescriptor);

    return true;
}
