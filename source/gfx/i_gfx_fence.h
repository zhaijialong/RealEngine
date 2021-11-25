#pragma once

#include "i_gfx_resource.h"

class IGfxFence : public IGfxResource
{
public:
    virtual ~IGfxFence() {}

    virtual void Wait(uint64_t value) = 0;
    virtual void Signal(uint64_t value) = 0;
};