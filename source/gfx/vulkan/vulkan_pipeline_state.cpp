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
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);

    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = (VkShaderModule)m_desc.vs->GetHandle();
    stages[0].pName = m_desc.vs->GetDesc().entry_point.c_str();
    if (m_desc.ps)
    {
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = (VkShaderModule)m_desc.ps->GetHandle();
        stages[1].pName = m_desc.ps->GetDesc().entry_point.c_str();
    }

    VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = ToVkPrimitiveTopology(m_desc.primitive_type);
    
    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkDynamicState dynamicStates[4] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicStateInfo.dynamicStateCount = 4;
    dynamicStateInfo.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterState = ToVkPipelineRasterizationStateCreateInfo(m_desc.rasterizer_state);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = ToVkPipelineDepthStencilStateCreateInfo(m_desc.depthstencil_state);

    VkPipelineColorBlendAttachmentState blendStates[8];
    VkPipelineColorBlendStateCreateInfo colorBlendState = ToVkPipelineColorBlendStateCreateInfo(m_desc.blend_state, blendStates);

    VkFormat colorFormats[8];
    VkPipelineRenderingCreateInfo formatInfo = ToVkPipelineRenderingCreateInfo(m_desc, colorFormats);

    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    createInfo.pNext = &formatInfo;
    createInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    createInfo.stageCount = m_desc.ps ? 2 : 1;
    createInfo.pStages = stages;
    createInfo.pVertexInputState = &vertexInput;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pViewportState = &viewportInfo;
    createInfo.pRasterizationState = &rasterState;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicStateInfo;
    createInfo.layout = ((VulkanDevice*)m_pDevice)->GetPipelineLayout();

    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanGraphicsPipelineState] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, m_pipeline, m_name.c_str());

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
    ((VulkanDevice*)m_pDevice)->Delete(m_pipeline);

    uint32_t stageCount = 1;
    VkPipelineShaderStageCreateInfo stages[3];
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[0].module = (VkShaderModule)m_desc.ms->GetHandle();
    stages[0].pName = m_desc.ms->GetDesc().entry_point.c_str();

    if (m_desc.as)
    {
        stages[stageCount] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[stageCount].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
        stages[stageCount].module = (VkShaderModule)m_desc.as->GetHandle();
        stages[stageCount].pName = m_desc.as->GetDesc().entry_point.c_str();
        ++stageCount;
    }

    if (m_desc.ps)
    {
        stages[stageCount] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[stageCount].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[stageCount].module = (VkShaderModule)m_desc.ps->GetHandle();
        stages[stageCount].pName = m_desc.ps->GetDesc().entry_point.c_str();
        ++stageCount;
    }

    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkDynamicState dynamicStates[4] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicStateInfo.dynamicStateCount = 4;
    dynamicStateInfo.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterState = ToVkPipelineRasterizationStateCreateInfo(m_desc.rasterizer_state);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = ToVkPipelineDepthStencilStateCreateInfo(m_desc.depthstencil_state);

    VkPipelineColorBlendAttachmentState blendStates[8];
    VkPipelineColorBlendStateCreateInfo colorBlendState = ToVkPipelineColorBlendStateCreateInfo(m_desc.blend_state, blendStates);

    VkFormat colorFormats[8];
    VkPipelineRenderingCreateInfo formatInfo = ToVkPipelineRenderingCreateInfo(m_desc, colorFormats);

    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    createInfo.pNext = &formatInfo;
    createInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    createInfo.stageCount = stageCount;
    createInfo.pStages = stages;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pViewportState = &viewportInfo;
    createInfo.pRasterizationState = &rasterState;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicStateInfo;
    createInfo.layout = ((VulkanDevice*)m_pDevice)->GetPipelineLayout();

    VkDevice device = (VkDevice)m_pDevice->GetHandle();
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanMeshShadingPipelineState] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, m_pipeline, m_name.c_str());

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
    createInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
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
