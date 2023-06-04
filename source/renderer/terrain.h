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
    void Hardness(IGfxCommandList* pCommandList);
    void Erosion(IGfxCommandList* pCommandList);
    void Raymarch(IGfxCommandList* pCommandList, RGTexture* output);

    void Clear(IGfxCommandList* pCommandList);
    void Save();

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture2D> m_pInputTexture;
    eastl::unique_ptr<IGfxBuffer> m_pStagingBuffer;
    eastl::string m_savePath;

    eastl::unique_ptr<Texture2D> m_pHeightmap;
    eastl::unique_ptr<Texture2D> m_pSediment0;
    eastl::unique_ptr<Texture2D> m_pSediment1;
    eastl::unique_ptr<Texture2D> m_pWater;
    eastl::unique_ptr<Texture2D> m_pFlux;
    eastl::unique_ptr<Texture2D> m_pVelocity;
    eastl::unique_ptr<Texture2D> m_pSoilFlux;
    eastl::unique_ptr<Texture2D> m_pHardness;
    eastl::unique_ptr<Texture2D> m_pHardnessBlurTemp;
   
    IGfxPipelineState* m_pHeightmapPSO = nullptr;
    IGfxPipelineState* m_pErosionPSO = nullptr;
    IGfxPipelineState* m_pRaymachPSO = nullptr;

    IGfxPipelineState* m_pHardnessPSO = nullptr;
    IGfxPipelineState* m_pBlurPSO = nullptr;
};
