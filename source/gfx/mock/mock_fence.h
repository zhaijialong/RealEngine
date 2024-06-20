#pragma once

#include "../gfx_fence.h"

class MockDevice;

class MockFence : public IGfxFence
{
public:
    MockFence(MockDevice* pDevice, const eastl::string& name);
    ~MockFence();

    bool Create();

    virtual void* GetHandle() const override;
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;
};