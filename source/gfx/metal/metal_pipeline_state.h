#pragma once

#include "../gfx_pipeline_state.h"

class MetalGraphicsPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class MetalMeshShadingPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class MetalComputePipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};