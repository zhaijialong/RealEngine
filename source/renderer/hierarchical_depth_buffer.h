#pragma once

#include "render_graph.h"
#include "resource/typed_buffer.h"

class Renderer;

class HZB
{
public:
    HZB(Renderer* pRenderer);

    void Generate1stPhaseCullingHZB(RenderGraph* graph);
    void Generate2ndPhaseCullingHZB(RenderGraph* graph, RGHandle depthRT);
    void GenerateSceneHZB(RenderGraph* graph, RGHandle depthRT);

    uint32_t GetHZBMipCount() const { return m_nHZBMipCount; }
    uint32_t GetHZBWidth() const { return m_hzbSize.x; };
    uint32_t GetHZBHeight() const { return m_hzbSize.y; }

    RGHandle Get1stPhaseCullingHZBMip(uint32_t mip) const;
    RGHandle Get2ndPhaseCullingHZBMip(uint32_t mip) const;
    RGHandle GetSceneHZBMip(uint32_t mip) const;

private:
    void CalcHZBSize();

    void ReprojectDepth(IGfxCommandList* pCommandList, RGTexture* reprojectedDepthTexture);
    void DilateDepth(IGfxCommandList* pCommandList, RGTexture* reprojectedDepthSRV, RGTexture* hzbMip0UAV);
    void BuildHZB(IGfxCommandList* pCommandList, RGTexture* texture, bool min_max = false);
    void InitHZB(IGfxCommandList* pCommandList, RGTexture* inputDepthSRV, RGTexture* hzbMip0UAV, bool min_max = false);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pDepthReprojectionPSO = nullptr;
    IGfxPipelineState* m_pDepthDilationPSO = nullptr;
    IGfxPipelineState* m_pDepthMipFilterPSO = nullptr;
    IGfxPipelineState* m_pInitHZBPSO = nullptr;

    IGfxPipelineState* m_pInitSceneHZBPSO = nullptr;
    IGfxPipelineState* m_pDepthMipFilterMinMaxPSO = nullptr;

    uint32_t m_nHZBMipCount = 0;
    uint2 m_hzbSize;

    static const uint32_t MAX_HZB_MIP_COUNT = 13; //spd limits
    RGHandle m_1stPhaseCullingHZBMips[MAX_HZB_MIP_COUNT] = {};
    RGHandle m_2ndPhaseCullingHZBMips[MAX_HZB_MIP_COUNT] = {};
    RGHandle m_sceneHZBMips[MAX_HZB_MIP_COUNT] = {};
};