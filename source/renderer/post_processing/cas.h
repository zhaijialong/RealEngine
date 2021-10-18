#pragma once

#include "../render_graph.h"

struct CASPassData
{
    RenderGraphHandle inRT;
    RenderGraphHandle outRT;
};

class CAS
{
public:
    CAS(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    float m_sharpenVal = 0.5f;
};