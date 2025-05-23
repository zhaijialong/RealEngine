#pragma once

#include "../gfx_heap.h"

class WebGPUDevice;

class WebGPUHeap : public IGfxHeap
{
public:
    WebGPUHeap(WebGPUDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name);
    ~WebGPUHeap();

    bool Create();

    virtual void* GetHandle() const override;
};