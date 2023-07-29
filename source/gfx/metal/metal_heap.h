#pragma once

#include "../gfx_heap.h"

class MetalHeap : public IGfxHeap
{
public:
    virtual void* GetHandle() const override;

};