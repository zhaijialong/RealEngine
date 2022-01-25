#pragma once

#include "../render_graph.h"

class ClusteredShading
{
public:
    ClusteredShading(Renderer* pRenderer);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};