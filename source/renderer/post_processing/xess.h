#pragma once

#include "core/platform.h"

#if RE_PLATFORM_WINDOWS

#include "../render_graph.h"
#include "xess/xess.h"

class XeSS
{
public:
    XeSS(Renderer* pRenderer);
    ~XeSS();

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);

    float GetUpscaleRatio(uint32_t displayWidth, uint32_t displayHeight) const;

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void InitializeXeSS(uint32_t displayWidth, uint32_t displayHeight);

private:
    Renderer* m_pRenderer = nullptr;

    xess_context_handle_t m_context = nullptr;
    xess_quality_settings_t m_quality = XESS_QUALITY_SETTING_QUALITY;
    bool m_needInitialization = true;
};

#endif // RE_PLATFORM_WINDOWS
