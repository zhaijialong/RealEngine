#pragma once

#include "render_graph.h"

class Renderer;

class HZBOcclusionCulling
{
public:
    HZBOcclusionCulling(Renderer* pRenderer);

    void GenerateHZB(RenderGraph* graph);

    RenderGraphHandle GetHZB() const { return m_hzb; }
    //uint32_t GetHZBMipCount() const;
    //RenderGraphHandle GetHZBMip(uint32_t mip) const;

private:
    void CalcHZBSize();

    void ReprojectDepth(IGfxCommandList* pCommandList, IGfxDescriptor* prevLinearDepthSRV, IGfxDescriptor* reprojectedDepthUAV);
    void DilateDepth(IGfxCommandList* pCommandList, IGfxDescriptor* reprojectedDepthSRV, IGfxDescriptor* hzbMip0UAV);

private:
    Renderer* m_pRenderer = nullptr;

    IGfxPipelineState* m_pDepthReprojectionPSO = nullptr;
    IGfxPipelineState* m_pDepthDilationPSO = nullptr;
    IGfxPipelineState* m_pDepthMipFilterPSO = nullptr;

    uint32_t m_nHZBMipCount = 0;
    uint2 m_hzbSize;

    //RenderGraphHandle m_hzbMips[10];
    RenderGraphHandle m_hzb;
};