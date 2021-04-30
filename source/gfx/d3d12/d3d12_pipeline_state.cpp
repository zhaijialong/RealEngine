#include "d3d12_pipeline_state.h"
#include "d3d12_device.h"

D3D12GraphicsPipelineState::D3D12GraphicsPipelineState(D3D12Device* pDevice, const GfxGraphicsPipelineDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
}

D3D12GraphicsPipelineState::~D3D12GraphicsPipelineState()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12GraphicsPipelineState::Create()
{
    return false;
}
