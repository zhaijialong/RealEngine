#pragma once

#include "gfx/gfx.h"

class RenderGraphResourceAllocator
{
public:
    void Reset();

    IGfxTexture* AllocateTexture(const GfxTextureDesc& desc, const std::string& name);

private:

};