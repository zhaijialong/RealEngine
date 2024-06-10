#include "vulkan_shader.h"
#include "vulkan_device.h"
#include "utils/log.h"
#include "xxHash/xxhash.h"

VulkanShader::VulkanShader(VulkanDevice* pDevice, const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;

    SetShaderData(data.data(), (uint32_t)data.size());
}

VulkanShader::~VulkanShader()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_shader);
}

bool VulkanShader::SetShaderData(const uint8_t* data, uint32_t data_size)
{
    ((VulkanDevice*)m_pDevice)->Delete(m_shader);

    m_hash = XXH3_64bits(data, data_size);

    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = (size_t)data_size;
    createInfo.pCode = (const uint32_t*)data;

    VkResult result = VK_SUCCESS; //todo vkCreateShaderModule(device, &createInfo, nullptr, &m_shader);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanShader] failed to create {}", m_name);
        return false;
    }

    //todo SetDebugName(device, VK_OBJECT_TYPE_SHADER_MODULE, m_shader, m_name.c_str());

    return true;
}
