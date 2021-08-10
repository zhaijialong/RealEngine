#pragma once

#include "gfx/gfx.h"

class Renderer;
class Texture2D;

class FXAA
{
public:
    FXAA(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, Texture2D* pLdrRT);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};