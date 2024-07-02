#pragma once

#include "Metal/MTLComputePipeline.hpp"
#include "../gfx_pipeline_state.h"

class MetalDevice;

class MetalGraphicsPipelineState : public IGfxPipelineState
{
public:
    MetalGraphicsPipelineState(MetalDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name);
    ~MetalGraphicsPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxGraphicsPipelineDesc m_desc;
};

class MetalMeshShadingPipelineState : public IGfxPipelineState
{
public:
    MetalMeshShadingPipelineState(MetalDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name);
    ~MetalMeshShadingPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxMeshShadingPipelineDesc m_desc;
};

class MetalComputePipelineState : public IGfxPipelineState
{
public:
    MetalComputePipelineState(MetalDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name);
    ~MetalComputePipelineState();

    virtual void* GetHandle() const override { return m_pPSO; }
    virtual bool Create() override;

private:
    GfxComputePipelineDesc m_desc;
    MTL::ComputePipelineState* m_pPSO = nullptr;
};
