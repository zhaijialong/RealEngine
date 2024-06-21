#include "mock_pipeline_state.h"
#include "mock_device.h"

MockGraphicsPipelineState::MockGraphicsPipelineState(MockDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
}

MockGraphicsPipelineState::~MockGraphicsPipelineState()
{
}

void* MockGraphicsPipelineState::GetHandle() const
{
    return nullptr;
}

bool MockGraphicsPipelineState::Create()
{
    return true;
}

MockMeshShadingPipelineState::MockMeshShadingPipelineState(MockDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

MockMeshShadingPipelineState::~MockMeshShadingPipelineState()
{
}

void* MockMeshShadingPipelineState::GetHandle() const
{
    return nullptr;
}

bool MockMeshShadingPipelineState::Create()
{
    return true;
}

MockComputePipelineState::MockComputePipelineState(MockDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

MockComputePipelineState::~MockComputePipelineState()
{
}

void* MockComputePipelineState::GetHandle() const
{
    return nullptr;
}

bool MockComputePipelineState::Create()
{
    return true;
}
