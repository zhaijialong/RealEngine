#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ShadowDenoiser
{
public:
    ShadowDenoiser(Renderer* pRenderer);

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depthRT, RGHandle normalRT, RGHandle velocityRT, uint32_t width, uint32_t height);

    void InvalidateHistory();

private:
    void Prepare(IGfxCommandList* pCommandList, RGTexture* raytraceResult, RGBuffer* maskBuffer, uint32_t width, uint32_t height);
    void TileClassification(IGfxCommandList* pCommandList, RGBuffer* shadowMaskBuffer, RGTexture* depthTexture, RGTexture* normalTexture,
        RGTexture* velocityTexture, RGBuffer* tileMetaDataBufferUAV, RGTexture* reprojectionResultTextureUAV, uint32_t width, uint32_t height);
    void Filter(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, RGTexture* depthTexture, RGTexture* normalTexture,
        RGBuffer* tileMetaDataBuffer, uint32_t pass_index, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPrepareMaskPSO = nullptr;
    IGfxPipelineState* m_pTileClassificationPSO = nullptr;
    IGfxPipelineState* m_pFilterPSO[3] = {};

    eastl::unique_ptr<Texture2D> m_pMomentsTexture;
    eastl::unique_ptr<Texture2D> m_pPrevMomentsTexture;
    eastl::unique_ptr<Texture2D> m_pHistoryTexture;

    bool m_bHistoryInvalid = true;
};