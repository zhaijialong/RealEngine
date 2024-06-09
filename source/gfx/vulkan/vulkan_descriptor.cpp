#include "vulkan_descriptor.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"

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
}

bool VulkanSampler::Create()
{
    return true;
}
