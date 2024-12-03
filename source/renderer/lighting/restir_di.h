#pragma once

#include "../render_graph.h"

// https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf

class ReSTIRDI
{
public:
    ReSTIRDI(Renderer* pRenderer);
    ~ReSTIRDI();
};