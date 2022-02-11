#pragma once

#include "../render_graph.h"

struct FXAAPassData
{
    RenderGraphHandle inRT;
    RenderGraphHandle outRT;
};

class FXAA
{
public:
    FXAA(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle inputHandle, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
};