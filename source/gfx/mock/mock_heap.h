#pragma once

#include "../gfx_heap.h"

class MockDevice;

class MockHeap : public IGfxHeap
{
public:
    MockHeap(MockDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name);
    ~MockHeap();

    bool Create();

    virtual void* GetHandle() const override;
};