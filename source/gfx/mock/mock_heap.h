#pragma once

#include "../gfx_heap.h"

class MockHeap : public IGfxHeap
{
public:
    virtual void* GetHandle() const override;

};