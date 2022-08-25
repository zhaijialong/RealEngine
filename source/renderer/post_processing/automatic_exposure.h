#pragma once

#include "../render_graph.h"
#include "../resource/typed_buffer.h"
#include "../resource/texture_2d.h"

class AutomaticExposure
{
public:
    AutomaticExposure(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColorRT, uint32_t width, uint32_t height);

private:
    void ComputeLuminanceSize(uint32_t width, uint32_t height);
    void InitLuminance(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output);
    void ReduceLuminance(IGfxCommandList* pCommandList, RGTexture* texture);

    void BuildHistogram(IGfxCommandList* pCommandList, RGTexture* inputTexture, RGBuffer* histogramBuffer, uint32_t width, uint32_t height);
    void ReduceHistogram(IGfxCommandList* pCommandList, RGBuffer* histogramBufferSRV, RGTexture* avgLuminanceUAV);

    void Exposure(IGfxCommandList* pCommandList, RGTexture* avgLuminance, RGTexture* output);

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_pLuminanceReductionPSO = nullptr;
    IGfxPipelineState* m_pHistogramReductionPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pPreviousEV100;

    enum class ExposureMode
    {
        Automatic,
        AutomaticHistogram,
        Manual,
    };

    enum class MeteringMode
    {
        Average,
        Spot,
        CenterWeighted,
    };

    ExposureMode m_exposuremode = ExposureMode::AutomaticHistogram;
    MeteringMode m_meteringMode = MeteringMode::CenterWeighted;

    uint2 m_luminanceSize;
    uint32_t m_luminanceMips = 0;

    float m_minLuminance = 0.0f;
    float m_maxLuminance = 10.0f;
    float m_adaptionSpeed = 1.5f;
    float m_histogramLowPercentile = 0.1f;
    float m_histogramHighPercentile = 0.9f;

    bool m_bHistoryInvalid = true;
    bool m_bDebugEV100 = false;
};