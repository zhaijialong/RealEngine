#pragma once

#include "d3d12_header.h"
#include "../i_gfx_pipeline_state.h"

class D3D12Device;

class D3D12GraphicsPipelineState : public IGfxPipelineState
{
public:
    D3D12GraphicsPipelineState(D3D12Device* pDevice, const GfxGraphicsPipelineDesc& desc, const std::string& name);
    ~D3D12GraphicsPipelineState();

    virtual void* GetHandle() const { return m_pPipelineState; }
    bool Create();
private:
    ID3D12PipelineState* m_pPipelineState = nullptr;
    GfxGraphicsPipelineDesc m_desc;
};

class D3D12MeshShadingPipelineState : public IGfxPipelineState
{
};