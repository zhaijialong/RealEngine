#pragma once

#include "core/platform.h"
#if RE_PLATFORM_MAC || RE_PLATFORM_IOS

#include "../render_graph.h"

namespace MTLFX { class TemporalScaler; }

class MetalFXTemporalUpscaler
{
public:
    MetalFXTemporalUpscaler(Renderer* renderer);
    ~MetalFXTemporalUpscaler();
    
    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);
    
    float GetUpscaleRatio() const { return m_upscaleRatio; }
    
private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);
    void CreateUpscaler(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);
    
private:
    Renderer* m_pRenderer = nullptr;
    MTLFX::TemporalScaler* m_pUpscaler = nullptr;
    float m_upscaleRatio = 2.0;
    bool m_needCreateUpscaler = true;
};

#endif // RE_PLATFORM_MAC || RE_PLATFORM_IOS
