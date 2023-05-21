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
    void Erosion(IGfxCommandList* pCommandList);
    void Raymarch(IGfxCommandList* pCommandList, RGTexture* output);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture2D> m_pHeightmap;
    eastl::unique_ptr<Texture2D> m_pHardness;
    eastl::unique_ptr<Texture2D> m_pSediment;
    eastl::unique_ptr<Texture2D> m_pWater;
    eastl::unique_ptr<Texture2D> m_pFlux;
    eastl::unique_ptr<Texture2D> m_pVelocity;
   
    IGfxPipelineState* m_pHeightmapPSO = nullptr;
    IGfxPipelineState* m_pErosionPSO = nullptr;
    IGfxPipelineState* m_pRaymachPSO = nullptr;
};