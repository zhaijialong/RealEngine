#pragma once

#include "../gfx_heap.h"

class VulkanHeap : public IGfxHeap
{
public:
    virtual void* GetHandle() const override;

};