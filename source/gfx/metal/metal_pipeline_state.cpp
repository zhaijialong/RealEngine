#include "metal_pipeline_state.h"
#include "metal_device.h"
#include "metal_shader.h"
#include "metal_utils.h"
#include "utils/log.h"

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
    m_pPSO->release();
}

bool MetalComputePipelineState::Create()
{
    if(m_pPSO)
    {
        m_pPSO->release();
    }
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    NS::Error* pError = nullptr;
    m_pPSO = device->newComputePipelineState((MTL::Function*)m_desc.cs->GetHandle(), &pError);

    if(!m_pPSO)
    {
        RE_ERROR("[MetalComputePipelineState] failed to create {} \r\n{}", m_name, pError->localizedDescription()->utf8String());
        pError->release();
        return false;
    }
    
    return true;
}
