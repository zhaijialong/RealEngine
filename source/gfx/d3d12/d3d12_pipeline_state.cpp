#include "d3d12_pipeline_state.h"
#include "d3d12_device.h"
#include "d3d12_shader.h"

D3D12GraphicsPipelineState::D3D12GraphicsPipelineState(D3D12Device* pDevice, const GfxGraphicsPipelineDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
    m_primitiveTopology = d3d12_primitive_topology(desc.primitive_type);
}

D3D12GraphicsPipelineState::~D3D12GraphicsPipelineState()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12GraphicsPipelineState::Create()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = ((D3D12Device*)m_pDevice)->GetRootSignature();

    desc.VS = ((D3D12Shader*)m_desc.vs)->GetByteCode();
    if (m_desc.ps)
    {
        desc.PS = ((D3D12Shader*)m_desc.ps)->GetByteCode();
    }

    desc.RasterizerState = d3d12_rasterizer_desc(m_desc.rasterizer_state);
    desc.DepthStencilState = d3d12_depth_stencil_desc(m_desc.depthstencil_state);
    desc.BlendState = d3d12_blend_desc(m_desc.blend_state);

    desc.NumRenderTargets = 8;
    for (int i = 0; i < 8; ++i)
    {
        desc.RTVFormats[i] = dxgi_format(m_desc.rt_format[i]);
    }
    desc.DSVFormat = dxgi_format(m_desc.depthstencil_format);
    desc.PrimitiveTopologyType = d3d12_topology_type(m_desc.primitive_type);
    desc.SampleMask = 0xFFFFFFFF;
    desc.SampleDesc.Count = 1;

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
    if (FAILED(pDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pPipelineState))))
    {
        return false;
    }

    m_pPipelineState->SetName(string_to_wstring(m_name).c_str());

    return true;
}
