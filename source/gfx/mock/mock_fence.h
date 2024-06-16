#pragma once

#include "../gfx_fence.h"

class MockFence : public IGfxFence
{
public:
    virtual void* GetHandle() const override;
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override;
};