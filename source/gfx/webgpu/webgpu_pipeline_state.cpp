#include "webgpu_pipeline_state.h"
#include "webgpu_device.h"

WebGPUGraphicsPipelineState::WebGPUGraphicsPipelineState(WebGPUDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
}

WebGPUGraphicsPipelineState::~WebGPUGraphicsPipelineState()
{
}

void* WebGPUGraphicsPipelineState::GetHandle() const
{
    return nullptr;
}

bool WebGPUGraphicsPipelineState::Create()
{
    return true;
}

WebGPUMeshShadingPipelineState::WebGPUMeshShadingPipelineState(WebGPUDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

WebGPUMeshShadingPipelineState::~WebGPUMeshShadingPipelineState()
{
}

void* WebGPUMeshShadingPipelineState::GetHandle() const
{
    return nullptr;
}

bool WebGPUMeshShadingPipelineState::Create()
{
    return true;
}

WebGPUComputePipelineState::WebGPUComputePipelineState(WebGPUDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

WebGPUComputePipelineState::~WebGPUComputePipelineState()
{
}

void* WebGPUComputePipelineState::GetHandle() const
{
    return nullptr;
}

bool WebGPUComputePipelineState::Create()
{
    return true;
}
