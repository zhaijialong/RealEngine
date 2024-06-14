#pragma once

#include "vulkan_header.h"
#include "../gfx_pipeline_state.h"

class VulkanDevice;

class VulkanGraphicsPipelineState : public IGfxPipelineState
{
public:
    VulkanGraphicsPipelineState(VulkanDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name);
    ~VulkanGraphicsPipelineState();

    virtual void* GetHandle() const override { return m_pipeline; }
    virtual bool Create() override;

private:
    GfxGraphicsPipelineDesc m_desc;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

class VulkanMeshShadingPipelineState : public IGfxPipelineState
{
public:
    VulkanMeshShadingPipelineState(VulkanDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name);
    ~VulkanMeshShadingPipelineState();

    virtual void* GetHandle() const override { return m_pipeline; }
    virtual bool Create() override;

private:
    GfxMeshShadingPipelineDesc m_desc;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

class VulkanComputePipelineState : public IGfxPipelineState
{
public:
    VulkanComputePipelineState(VulkanDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name);
    ~VulkanComputePipelineState();

    virtual void* GetHandle() const override { return m_pipeline; }
    virtual bool Create() override;

private:
    GfxComputePipelineDesc m_desc;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};