#include "vulkan_pipeline_state.h"
#include "vulkan_device.h"
#include "vulkan_shader.h"
#include "utils/log.h"

VulkanGraphicsPipelineState::VulkanGraphicsPipelineState(VulkanDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
}

VulkanGraphicsPipelineState::~VulkanGraphicsPipelineState()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);
}

bool VulkanGraphicsPipelineState::Create()
{
    return true;
}

VulkanMeshShadingPipelineState::VulkanMeshShadingPipelineState(VulkanDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

VulkanMeshShadingPipelineState::~VulkanMeshShadingPipelineState()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);
}

bool VulkanMeshShadingPipelineState::Create()
{
    return true;
}

VulkanComputePipelineState::VulkanComputePipelineState(VulkanDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

VulkanComputePipelineState::~VulkanComputePipelineState()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);
}

bool VulkanComputePipelineState::Create()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);

    VkComputePipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    createInfo.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    createInfo.stage.module = (VkShaderModule)m_desc.cs->GetHandle();
    createInfo.stage.pName = m_desc.cs->GetDesc().entry_point.c_str();
    createInfo.layout = ((VulkanDevice*)m_pDevice)->GetPipelineLayout();

    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();
    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanComputePipelineState] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, m_pipeline, m_name.c_str());

    return true;
}
