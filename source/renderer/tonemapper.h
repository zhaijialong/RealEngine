#pragma once

#include "gfx/gfx.h"

class Renderer;

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrRT);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};