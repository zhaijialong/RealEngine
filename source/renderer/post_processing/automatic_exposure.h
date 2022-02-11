#pragma once

#include "../render_graph.h"

class AutomaticExposure
{
public:
    AutomaticExposure(Renderer* pRenderer);

private:
    Renderer* m_pRenderer;
};