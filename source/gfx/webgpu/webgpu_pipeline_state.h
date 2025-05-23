#pragma once

#include "../gfx_pipeline_state.h"

class WebGPUDevice;

class WebGPUGraphicsPipelineState : public IGfxPipelineState
{
public:
    WebGPUGraphicsPipelineState(WebGPUDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name);
    ~WebGPUGraphicsPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxGraphicsPipelineDesc m_desc;
};

class WebGPUMeshShadingPipelineState : public IGfxPipelineState
{
public:
    WebGPUMeshShadingPipelineState(WebGPUDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name);
    ~WebGPUMeshShadingPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxMeshShadingPipelineDesc m_desc;
};

class WebGPUComputePipelineState : public IGfxPipelineState
{
public:
    WebGPUComputePipelineState(WebGPUDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name);
    ~WebGPUComputePipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxComputePipelineDesc m_desc;
};