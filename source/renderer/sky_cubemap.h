#pragma once

#include "render_graph.h"
#include "resource/texture_2d.h"
#include "resource/texture_cube.h"
#include "resource/typed_buffer.h"

class SkyCubeMap
{
public:
    SkyCubeMap(Renderer* pRenderer);

    void Update(IGfxCommandList* pCommandList);

    TextureCube* GetCubeTexture() const { return m_pTexture.get(); }
    TextureCube* GetSpecularCubeTexture() const { return m_pSpecularTexture.get(); }
    TextureCube* GetDiffuseCubeTexture() const { return m_pDiffuseTexture.get(); }

private:
    void UpdateCubeTexture(IGfxCommandList* pCommandList);
    void UpdateSpecularCubeTexture(IGfxCommandList* pCommandList);
    void UpdateDiffuseCubeTexture(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
    IGfxPipelineState* m_pTexturedSkyPSO = nullptr;
    IGfxPipelineState* m_pRealtimeSkyPSO = nullptr;
    IGfxPipelineState* m_pGenerateMipsPSO = nullptr;
    IGfxPipelineState* m_pSpecularFilterPSO = nullptr;
    IGfxPipelineState* m_pDiffuseFilterPSO = nullptr;

    eastl::unique_ptr<TypedBuffer> m_pSPDCounterBuffer;
    eastl::unique_ptr<Texture2D> m_pHDRITexture;
    eastl::unique_ptr<TextureCube> m_pTexture;
    eastl::unique_ptr<TextureCube> m_pSpecularTexture;
    eastl::unique_ptr<TextureCube> m_pDiffuseTexture;

    enum class SkySource
    {
        Realtime,
        HDRI
    };

    SkySource m_source = SkySource::HDRI;

    bool m_bDirty =  true;
    float3 m_prevLightDir;
    float3 m_prevLightColor;
    float m_prevLightIntensity = 0.0f;

    eastl::unique_ptr<Texture2D> m_pPendingHDRITexture;
};