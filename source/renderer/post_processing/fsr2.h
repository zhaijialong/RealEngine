#pragma once

#include "../render_graph.h"

class FSR2
{
public:
    FSR2(Renderer* pRenderer);

private:
    Renderer* m_pRenderer = nullptr;
};
