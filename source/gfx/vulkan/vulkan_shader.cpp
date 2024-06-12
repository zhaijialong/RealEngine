#include "vulkan_shader.h"
#include "vulkan_device.h"
#include "utils/log.h"
#include "xxHash/xxhash.h"

VulkanShader::VulkanShader(VulkanDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanShader::~VulkanShader()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_shader);
}

bool VulkanShader::Create(eastl::span<uint8_t> data)
{
    ((VulkanDevice*)m_pDevice)->Delete(m_shader);

    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = data.size();
    createInfo.pCode = (const uint32_t*)data.data();

    VkResult result = VK_SUCCESS; //todo vkCreateShaderModule(device, &createInfo, nullptr, &m_shader);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanShader] failed to create {}", m_name);
        return false;
    }

    //todo SetDebugName(device, VK_OBJECT_TYPE_SHADER_MODULE, m_shader, m_name.c_str());

    m_hash = XXH3_64bits(data.data(), data.size());

    return true;
}
