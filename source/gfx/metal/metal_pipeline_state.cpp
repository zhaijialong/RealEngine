#include "metal_pipeline_state.h"
#include "metal_device.h"

MetalGraphicsPipelineState::MetalGraphicsPipelineState(MetalDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
}

MetalGraphicsPipelineState::~MetalGraphicsPipelineState()
{
}

void* MetalGraphicsPipelineState::GetHandle() const
{
    return nullptr;
}

bool MetalGraphicsPipelineState::Create()
{
    return true;
}

MetalMeshShadingPipelineState::MetalMeshShadingPipelineState(MetalDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

MetalMeshShadingPipelineState::~MetalMeshShadingPipelineState()
{
}

void* MetalMeshShadingPipelineState::GetHandle() const
{
    return nullptr;
}

bool MetalMeshShadingPipelineState::Create()
{
    return true;
}

MetalComputePipelineState::MetalComputePipelineState(MetalDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

MetalComputePipelineState::~MetalComputePipelineState()
{
}

void* MetalComputePipelineState::GetHandle() const
{
    return nullptr;
}

bool MetalComputePipelineState::Create()
{
    return true;
}
