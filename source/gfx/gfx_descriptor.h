#pragma once

#include "gfx_resource.h"

class IGfxDescriptor : public IGfxResource
{
public:
    virtual uint32_t GetHeapIndex() const = 0;
};