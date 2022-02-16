#pragma once

#include <stdint.h>

struct RenderGraphHandle
{
    uint16_t index = uint16_t(-1);
    uint16_t node = uint16_t(-1);

    bool IsValid() const
    {
        return index != uint16_t(-1) && node != uint16_t(-1);
    }
};
