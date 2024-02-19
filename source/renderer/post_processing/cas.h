#pragma once

#include "../render_graph.h"

class CAS
{
public:
    CAS(Renderer* pRenderer);

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle inputHandle, uint32_t width, uint32_t height);

private:
    void Render(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    bool m_bEnable = true;
    float m_sharpenVal = 0.5f;
};