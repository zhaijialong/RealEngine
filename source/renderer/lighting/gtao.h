#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

struct GTAOFilterDepthPassData
{
    RenderGraphHandle inputDepth;
    RenderGraphHandle outputDepthMip0;
    RenderGraphHandle outputDepthMip1;
    RenderGraphHandle outputDepthMip2;
    RenderGraphHandle outputDepthMip3;
    RenderGraphHandle outputDepthMip4;
};

struct GTAOPassData
{
    RenderGraphHandle inputFilteredDepth;
    RenderGraphHandle inputNormal;
    RenderGraphHandle outputAOTerm;
    RenderGraphHandle outputEdge;
};

struct GTAODenoisePassData
{
    RenderGraphHandle inputAOTerm;
    RenderGraphHandle inputEdge;
    RenderGraphHandle outputAOTerm;
};

class GTAO
{
public:
    GTAO(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, uint32_t width, uint32_t height);

private:
    void CreateHilbertLUT();
    void UpdateGTAOConstants(IGfxCommandList* pCommandList, uint32_t width, uint32_t height);

    void FilterDepth(IGfxCommandList* pCommandList, const GTAOFilterDepthPassData& data, uint32_t width, uint32_t height);
    void Draw(IGfxCommandList* pCommandList, const GTAOPassData& data, uint32_t width, uint32_t height);
    void Denoise(IGfxCommandList* pCommandList, const GTAODenoisePassData& data, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pPrefilterDepthPSO = nullptr;
    IGfxPipelineState* m_pGTAOLowPSO = nullptr;
    IGfxPipelineState* m_pGTAOMediumPSO = nullptr;
    IGfxPipelineState* m_pGTAOHighPSO = nullptr;
    IGfxPipelineState* m_pGTAOUltraPSO = nullptr;
    IGfxPipelineState* m_pDenoisePSO = nullptr;

    std::unique_ptr<Texture2D> m_pHilbertLUT;

    bool m_bEnable = true;
    int m_qualityLevel = 2;
    float m_radius = 0.5f;
};