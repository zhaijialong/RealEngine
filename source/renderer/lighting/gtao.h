#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class GTAO
{
public:
    GTAO(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, uint32_t width, uint32_t height);

private:
    void CreateHilbertLUT();
    void UpdateGTAOConstants(IGfxCommandList* pCommandList, uint32_t width, uint32_t height);

    void FilterDepth(IGfxCommandList* pCommandList, RGTexture* depth, RGTexture* hzb, uint32_t width, uint32_t height);
    void Draw(IGfxCommandList* pCommandList, RGTexture* hzb, RGTexture* normal, RGTexture* outputAO, RGTexture* outputEdge, uint32_t width, uint32_t height);
    void Denoise(IGfxCommandList* pCommandList, RGTexture* inputAO, RGTexture* edge, RGTexture* outputAO, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pPrefilterDepthPSO = nullptr;
    IGfxPipelineState* m_pGTAOLowPSO = nullptr;
    IGfxPipelineState* m_pGTAOMediumPSO = nullptr;
    IGfxPipelineState* m_pGTAOHighPSO = nullptr;
    IGfxPipelineState* m_pGTAOUltraPSO = nullptr;
    IGfxPipelineState* m_pDenoisePSO = nullptr;

    IGfxPipelineState* m_pGTAOSOLowPSO = nullptr;
    IGfxPipelineState* m_pGTAOSOMediumPSO = nullptr;
    IGfxPipelineState* m_pGTAOSOHighPSO = nullptr;
    IGfxPipelineState* m_pGTAOSOUltraPSO = nullptr;
    IGfxPipelineState* m_pSODenoisePSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pHilbertLUT;

    bool m_bEnable = true;
    bool m_bEnableGTSO = false;
    int m_qualityLevel = 2;
    float m_radius = 0.5f;
};