#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class NRDIntegration;

class GIDenoiserNRD
{
public:
    GIDenoiserNRD(Renderer* pRenderer);
    ~GIDenoiserNRD();

private:
    Renderer* m_pRenderer = nullptr;
    eastl::unique_ptr<NRDIntegration> m_pReblur;
};