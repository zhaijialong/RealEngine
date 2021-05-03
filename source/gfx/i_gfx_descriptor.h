#pragma once

#include "i_gfx_resource.h"

class IGfxDescriptor : public IGfxResource
{
public:
    virtual uint32_t GetIndex() const = 0;
};