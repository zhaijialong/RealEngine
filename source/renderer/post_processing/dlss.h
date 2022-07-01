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

    bool IsSupported() const { return m_ngxInitialized && m_dlssAvailable; }

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    bool InitializeNGX();
    void ShutdownNGX();

    bool InitializeDLSSFeatures(uint32_t displayWidth, uint32_t displayHeight);
    void ReleaseDLSSFeatures();

private:
    Renderer* m_pRenderer = nullptr;

    std::unique_ptr<IGfxCommandList> m_pDlssInitCommandList;

    NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
    NVSDK_NGX_Handle* m_dlssFeature = nullptr;
    bool m_ngxInitialized = false;
    bool m_dlssAvailable = false;

    int m_qualityMode = 2;// NVSDK_NGX_PerfQuality_Value_MaxQuality;
    float m_sharpness = 0.5f;
};