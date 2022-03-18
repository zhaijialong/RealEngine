#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class RTShadow
{
public:
    RTShadow(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle depthRT, RenderGraphHandle normalRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height);

private:
    void RayTrace(IGfxCommandList* pCommandList, IGfxDescriptor* depthSRV, IGfxDescriptor* normalSRV, IGfxDescriptor* shadowUAV, uint32_t width, uint32_t height);
    void DenoiserPrepare(IGfxCommandList* pCommandList, IGfxDescriptor* raytraceResult, IGfxDescriptor* maskBuffer, uint32_t width, uint32_t height);
    void DenoiserTileClassification(IGfxCommandList* pCommandList, IGfxDescriptor* shadowMaskBuffer, IGfxDescriptor* depthTexture, IGfxDescriptor* normalTexture,
        IGfxDescriptor* velocityTexture, IGfxDescriptor* tileMetaDataBufferUAV, IGfxDescriptor* reprojectionResultTextureUAV, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pRaytracePSO = nullptr;
    IGfxPipelineState* m_pPrepareMaskPSO = nullptr;
    IGfxPipelineState* m_pTileClassificationPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pMomentsTexture;
    eastl::unique_ptr<Texture2D> m_pPrevMomentsTexture;
    eastl::unique_ptr<Texture2D> m_pHistoryTexture;

    bool m_bEnableDenoiser = true;
    bool m_bHistoryInvalid = true;
};