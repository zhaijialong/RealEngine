#pragma once

#include "../gfx_pipeline_state.h"

class MockDevice;

class MockGraphicsPipelineState : public IGfxPipelineState
{
public:
    MockGraphicsPipelineState(MockDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name);
    ~MockGraphicsPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxGraphicsPipelineDesc m_desc;
};

class MockMeshShadingPipelineState : public IGfxPipelineState
{
public:
    MockMeshShadingPipelineState(MockDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name);
    ~MockMeshShadingPipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxMeshShadingPipelineDesc m_desc;
};

class MockComputePipelineState : public IGfxPipelineState
{
public:
    MockComputePipelineState(MockDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name);
    ~MockComputePipelineState();

    virtual void* GetHandle() const override;
    virtual bool Create() override;

private:
    GfxComputePipelineDesc m_desc;
};