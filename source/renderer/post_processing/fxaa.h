#pragma once

#include "../render_graph.h"

struct FXAAPassData
{
    RGHandle inRT;
    RGHandle outRT;
};

class FXAA
{
public:
    FXAA(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle inputHandle, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    bool m_bEnable = true;
};