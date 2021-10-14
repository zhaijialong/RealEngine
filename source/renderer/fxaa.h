#pragma once

#include "render_graph.h"

struct FXAAPassData
{
    RenderGraphHandle ldrRT;
    RenderGraphHandle outputRT;
};

class FXAA
{
public:
    FXAA(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};