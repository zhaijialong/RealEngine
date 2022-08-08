#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class ShadowDenoiser
{
public:
    ShadowDenoiser(Renderer* pRenderer);

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depthRT, RenderGraphHandle normalRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height);

private:
    void Prepare(IGfxCommandList* pCommandList, RenderGraphTexture* raytraceResult, RenderGraphBuffer* maskBuffer, uint32_t width, uint32_t height);
    void TileClassification(IGfxCommandList* pCommandList, RenderGraphBuffer* shadowMaskBuffer, RenderGraphTexture* depthTexture, RenderGraphTexture* normalTexture,
        RenderGraphTexture* velocityTexture, RenderGraphBuffer* tileMetaDataBufferUAV, RenderGraphTexture* reprojectionResultTextureUAV, uint32_t width, uint32_t height);
    void Filter(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, RenderGraphTexture* depthTexture, RenderGraphTexture* normalTexture,
        RenderGraphBuffer* tileMetaDataBuffer, uint32_t pass_index, uint32_t width, uint32_t height);

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