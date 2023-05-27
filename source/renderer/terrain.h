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

    void Clear(IGfxCommandList* pCommandList);
    void Save(const eastl::string& file);

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture2D> m_pInputTexture;
    eastl::unique_ptr<IGfxBuffer> m_pStagingBuffer;

    eastl::unique_ptr<Texture2D> m_pHeightmap0;
    eastl::unique_ptr<Texture2D> m_pHeightmap1;
    eastl::unique_ptr<Texture2D> m_pSediment0;
    eastl::unique_ptr<Texture2D> m_pSediment1;
    eastl::unique_ptr<Texture2D> m_pWater;
    eastl::unique_ptr<Texture2D> m_pFlux;
    eastl::unique_ptr<Texture2D> m_pVelocity0;
    eastl::unique_ptr<Texture2D> m_pVelocity1;
    eastl::unique_ptr<Texture2D> m_pRegolith;
    eastl::unique_ptr<Texture2D> m_pRegolithFlux;
   
    IGfxPipelineState* m_pHeightmapPSO = nullptr;
    IGfxPipelineState* m_pErosionPSO = nullptr;
    IGfxPipelineState* m_pRaymachPSO = nullptr;
};
