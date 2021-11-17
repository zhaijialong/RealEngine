#pragma once

#include "../i_gfx_heap.h"

class D3D12Device;

namespace D3D12MA
{
    class Allocation;
}

class D3D12Heap : public IGfxHeap
{
public:
    D3D12Heap(D3D12Device* pDevice, const GfxHeapDesc& desc, const std::string& name);
    ~D3D12Heap();

    virtual void* GetHandle() const override { return m_pAllocation; }

    bool Create();

private:
    D3D12MA::Allocation* m_pAllocation = nullptr;
};
