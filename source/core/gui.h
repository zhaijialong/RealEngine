#pragma once

#include "../gfx/gfx.h"

class GUI
{
public:
    GUI();
    ~GUI();

    bool Init();
    void NewFrame();
    void Render();

private:
    IGfxPipelineState* m_pPSO = nullptr;
};