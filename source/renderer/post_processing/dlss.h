#pragma once

#include "../render_graph.h"

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

class DLSS
{
public:
    DLSS(Renderer* pRenderer);
    ~DLSS();

    RenderGraphHandle Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depth, RenderGraphHandle velocity,
        RenderGraphHandle exposure, uint32_t displayWidth, uint32_t displayHeight);

    bool IsSupported() const { return m_dlssAvailable; }
    float GetUpscaleRatio() const;

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    bool InitializeNGX();
    void ShutdownNGX();

    bool InitializeDLSSFeatures(IGfxCommandList* pCommandList);
    void ReleaseDLSSFeatures();

    int GetQualityMode() const;

private:
    Renderer* m_pRenderer = nullptr;

    NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
    NVSDK_NGX_Handle* m_dlssFeature = nullptr;
    bool m_dlssAvailable = false;
    bool m_needInitializeDlss = true;

    int m_qualityMode = 2;// NVSDK_NGX_PerfQuality_Value_MaxQuality;
    float m_customUpscaleRatio = 1.0f;
    float m_sharpness = 0.5f;
};