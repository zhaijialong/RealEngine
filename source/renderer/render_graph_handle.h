#pragma once

#include <stdint.h>

struct RenderGraphHandle
{
    uint16_t index = uint16_t(-1);
    uint16_t version = 0;

    bool IsInitialized() const { return index != uint16_t(-1); }
};