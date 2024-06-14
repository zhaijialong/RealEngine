#include "vulkan_pipeline_state.h"
#include "vulkan_device.h"

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
    return false;
}

VulkanMeshShadingPipelineState::VulkanMeshShadingPipelineState(VulkanDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

VulkanMeshShadingPipelineState::~VulkanMeshShadingPipelineState()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);
}

bool VulkanMeshShadingPipelineState::Create()
{
    return false;
}

VulkanComputePipelineState::VulkanComputePipelineState(VulkanDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

VulkanComputePipelineState::~VulkanComputePipelineState()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);
}

bool VulkanComputePipelineState::Create()
{
    return false;
}
