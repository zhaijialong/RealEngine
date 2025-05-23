#pragma once

#include "../gfx_fence.h"

class WebGPUDevice;

class WebGPUFence : public IGfxFence
{
public:
    WebGPUFence(WebGPUDevice* pDevice, const eastl::string& name);
    ~WebGPUFence();

    bool Create();

    virtual void* GetHandle() const override;
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;
};