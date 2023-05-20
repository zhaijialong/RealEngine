#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"

class Terrain
{
public:
    Terrain(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle output);

private:
    void Heightmap(IGfxCommandList* pCommandList);
    void Raymarch(IGfxCommandList* pCommandList, RGTexture* output);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture2D> m_pHeightmap;
    bool m_bGenerateHeightmap = true;
    uint m_seed = 123456;

    IGfxPipelineState* m_pHeightmapPSO = nullptr;
    IGfxPipelineState* m_pRaymachPSO = nullptr;
};