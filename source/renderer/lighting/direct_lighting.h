#pragma once

#include "../render_graph.h"

class DirectLighting
{
public:
    DirectLighting(Renderer* pRenderer);
    ~DirectLighting();

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle diffuse, RGHandle specular, RGHandle normal,
        RGHandle customData, RGHandle depth, RGHandle shadow, uint32_t width, uint32_t height);

private:
    void Render(IGfxCommandList* pCommandList, RGTexture* diffuse, RGTexture* specular, RGTexture* normal,
        RGTexture* customData, RGTexture* depth, RGTexture* shadow, RGTexture* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<class ClusteredLightLists> m_pClusteredLightLists;
    eastl::unique_ptr<class TiledLightTrees> m_pTiledLightTrees;
    eastl::unique_ptr<class ReSTIRDI> m_pReSTIRDI;
};