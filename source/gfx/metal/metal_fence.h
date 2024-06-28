#pragma once

#include "../gfx_fence.h"

class MetalDevice;

class MetalFence : public IGfxFence
{
public:
    MetalFence(MetalDevice* pDevice, const eastl::string& name);
    ~MetalFence();

    bool Create();
    
    virtual void* GetHandle() const override;
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;
};
