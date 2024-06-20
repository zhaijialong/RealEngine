#pragma once

#include "../gfx_pipeline_state.h"

class MockGraphicsPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class MockMeshShadingPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class MockComputePipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};