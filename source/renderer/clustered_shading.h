#pragma once

#include "gfx/gfx.h"

class Renderer;

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};