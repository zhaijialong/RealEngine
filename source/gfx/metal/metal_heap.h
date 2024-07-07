#pragma once

#include "metal_utils.h"
#include "../gfx_heap.h"

class MetalDevice;

class MetalHeap : public IGfxHeap
{
public:
    MetalHeap(MetalDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name);
    ~MetalHeap();

    bool Create();
    
    virtual void* GetHandle() const override { return m_pHeap; }

private:
    MTL::Heap* m_pHeap = nullptr;
};
