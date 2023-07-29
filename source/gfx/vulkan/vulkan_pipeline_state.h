#pragma once

#include "../gfx_pipeline_state.h"

class VulkanGraphicsPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class VulkanMeshShadingPipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};

class VulkanComputePipelineState : public IGfxPipelineState
{
public:
    virtual void* GetHandle() const override;
    virtual bool Create() override;
};