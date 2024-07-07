#pragma once

#include "metal_utils.h"
#include "../gfx_fence.h"

class MetalDevice;

class MetalFence : public IGfxFence
{
public:
    MetalFence(MetalDevice* pDevice, const eastl::string& name);
    ~MetalFence();

    bool Create();
    
    virtual void* GetHandle() const override { return m_pEvent; }
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;
    
private:
    MTL::SharedEvent* m_pEvent = nullptr;
};
