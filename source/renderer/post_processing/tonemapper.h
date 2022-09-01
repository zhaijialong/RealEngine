#pragma once

#include "../render_graph.h"

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle inputHandle, RGHandle exposure, 
        RGHandle bloom, float bloom_intensity, uint32_t width, uint32_t height);

private:
    void Draw(IGfxCommandList* pCommandList, RGTexture* pHdrSRV, RGTexture* exposure, RGTexture* pLdrUAV,
        RGTexture* bloom, float bloom_intensity, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    enum class TonemappingMode
    {
        Neutral,
        ACES,
    };
    TonemappingMode m_mode = TonemappingMode::Neutral;

    bool m_bEnableDither = true;
};