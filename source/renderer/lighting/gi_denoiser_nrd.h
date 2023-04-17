#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class NRDIntegration;

class GIDenoiserNRD
{
public:
    GIDenoiserNRD(Renderer* pRenderer);
    ~GIDenoiserNRD();

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle radiance, RGHandle normal, RGHandle linearDepth, RGHandle velocity, uint32_t width, uint32_t height);

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);
    void CreateReblurDenoiser(uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    eastl::unique_ptr<NRDIntegration> m_pReblur;
    bool m_bNeedCreateReblur = true;

    IGfxPipelineState* m_pPackNormalRoughnessPSO = nullptr;
    IGfxPipelineState* m_pPackRadianceHitTPSO = nullptr;
    IGfxPipelineState* m_pUnpackOutputRadiancePSO = nullptr;
};