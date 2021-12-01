#include "d3d12_pipeline_state.h"
#include "d3d12_device.h"
#include "d3d12_shader.h"

template<class T>
inline bool has_rt_binding(const T& desc)
{
    for (int i = 0; i < 8; ++i)
    {
        if (desc.rt_format[i] != GfxFormat::Unknown)
        {
            return true;
        }
    }
    return false;
}

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

    desc.NumRenderTargets = has_rt_binding(m_desc) ? 8 : 0;
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

D3D12ComputePipelineState::D3D12ComputePipelineState(D3D12Device* pDevice, const GfxComputePipelineDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

D3D12ComputePipelineState::~D3D12ComputePipelineState()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12ComputePipelineState::Create()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = ((D3D12Device*)m_pDevice)->GetRootSignature();
    desc.CS = ((D3D12Shader*)m_desc.cs)->GetByteCode();

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
    if (FAILED(pDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pPipelineState))))
    {
        return false;
    }

    m_pPipelineState->SetName(string_to_wstring(m_name).c_str());

    return true;
}

D3D12MeshShadingPipelineState::D3D12MeshShadingPipelineState(D3D12Device* pDevice, const GfxMeshShadingPipelineDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

D3D12MeshShadingPipelineState::~D3D12MeshShadingPipelineState()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12MeshShadingPipelineState::Create()
{
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ((D3D12Device*)m_pDevice)->GetRootSignature();

    if (m_desc.as)
    {
        psoDesc.AS = ((D3D12Shader*)m_desc.as)->GetByteCode();
    }
    psoDesc.MS = ((D3D12Shader*)m_desc.ms)->GetByteCode();
    psoDesc.PS = ((D3D12Shader*)m_desc.ps)->GetByteCode();

    psoDesc.RasterizerState = d3d12_rasterizer_desc(m_desc.rasterizer_state);
    psoDesc.DepthStencilState = d3d12_depth_stencil_desc(m_desc.depthstencil_state);
    psoDesc.BlendState = d3d12_blend_desc(m_desc.blend_state);

    psoDesc.NumRenderTargets = has_rt_binding(m_desc) ? 8 : 0;
    for (int i = 0; i < 8; ++i)
    {
        psoDesc.RTVFormats[i] = dxgi_format(m_desc.rt_format[i]);
    }
    psoDesc.DSVFormat = dxgi_format(m_desc.depthstencil_format);
    psoDesc.SampleMask = 0xFFFFFFFF;
    psoDesc.SampleDesc.Count = 1;

    auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psoDesc);

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
    streamDesc.pPipelineStateSubobjectStream = &psoStream;
    streamDesc.SizeInBytes = sizeof(psoStream);

    ID3D12Device2* pDevice = (ID3D12Device2*)m_pDevice->GetHandle();
    HRESULT hr = pDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_pPipelineState));
    if (FAILED(hr))
    {
        return false;
    }

    m_pPipelineState->SetName(string_to_wstring(m_name).c_str());
    return true;
}
