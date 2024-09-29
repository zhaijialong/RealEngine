#pragma once

#include "core/platform.h"

#if RE_PLATFORM_WINDOWS

#include "../render_graph.h"

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

class DLSS
{
public:
    DLSS(Renderer* pRenderer);
    ~DLSS();

    void OnGui();
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure, 
        uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);

    bool IsSupported() const { return m_dlssAvailable; }
    float GetUpscaleRatio() const;

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    bool InitializeNGX();
    void ShutdownNGX();

    bool InitializeDLSSFeatures(IGfxCommandList* pCommandList, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);
    void ReleaseDLSSFeatures();

private:
    Renderer* m_pRenderer = nullptr;

    NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
    NVSDK_NGX_Handle* m_dlssFeature = nullptr;
    bool m_dlssAvailable = false;
    bool m_needInitializeDlss = true;

    int m_qualityMode = 2;// NVSDK_NGX_PerfQuality_Value_MaxQuality;
    float m_customUpscaleRatio = 1.0f;
};

#endif // #if RE_PLATFORM_WINDOWS
