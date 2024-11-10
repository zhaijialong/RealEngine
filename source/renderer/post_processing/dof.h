#pragma once

#include "../render_graph.h"

class DoF
{
public:
    DoF(Renderer* pRenderer);

    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height);

private:
    void AddDownsamplePass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height, RGHandle& downsampledFar, RGHandle& downsampledNear);
    RGHandle AddFarBlurPass(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height);
    RGHandle AddCocDilationPass(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height);
    RGHandle AddNearBlurPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle coc, uint32_t width, uint32_t height);
    RGHandle AddCompositePass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, RGHandle farBlur, RGHandle nearBlur, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pDownsamplePSO = nullptr;
    IGfxPipelineState* m_pFarHorizontalBlurPSO = nullptr;
    IGfxPipelineState* m_pFarVerticalBlurPSO = nullptr;
    IGfxPipelineState* m_pTileCocXPSO = nullptr;
    IGfxPipelineState* m_pTileCocYPSO = nullptr;
    IGfxPipelineState* m_pNearHorizontalBlurPSO = nullptr;
    IGfxPipelineState* m_pNearVerticalBlurPSO = nullptr;
    IGfxPipelineState* m_pCompositePSO = nullptr;

    bool m_bEnable = false;
    float m_focusDistance = 5.0f;
    float m_maxCocSize = 16.0f;
};