#pragma once

#include "../render_graph.h"

// https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf

class ClusteredLightLists
{
public:
    ClusteredLightLists(Renderer* pRenderer);
    ~ClusteredLightLists();
};