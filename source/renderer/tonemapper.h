#pragma once

#include "gfx/gfx.h"
#include "utils/math.h"

enum DisplayMode
{
    DISPLAYMODE_SDR,
    DISPLAYMODE_FSHDR_Gamma22,
    DISPLAYMODE_FSHDR_SCRGB,
    DISPLAYMODE_HDR10_2084,
    DISPLAYMODE_HDR10_SCRGB
};

enum ColorSpace
{
    ColorSpace_REC709,
    ColorSpace_P3,
    ColorSpace_REC2020,
    ColorSpace_Display
};

class Renderer;

class Tonemapper
{
public:
    Tonemapper(Renderer* pRenderer, DisplayMode display_mode, ColorSpace color_space);

    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrRT);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

private:
    // LPM Only
    void SetLPMConfig(bool con, bool soft, bool con2, bool clip, bool scaleOnly);
    void SetLPMColors(
        float xyRedW[2], float xyGreenW[2], float xyBlueW[2], float xyWhiteW[2],
        float xyRedO[2], float xyGreenO[2], float xyBlueO[2], float xyWhiteO[2],
        float xyRedC[2], float xyGreenC[2], float xyBlueC[2], float xyWhiteC[2],
        float scaleC
    );

    bool m_shoulder; // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
    bool m_con; // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
    bool m_soft; // Use soft gamut mapping.
    bool m_con2; // Use last RGB conversion matrix.
    bool m_clip; // Use clipping in last conversion matrix.
    bool m_scaleOnly; // Scale only for last conversion matrix (used for 709 HDR to scRGB).
    float m_xyRedW[2]; float m_xyGreenW[2]; float m_xyBlueW[2]; float m_xyWhiteW[2]; // Chroma coordinates for working color space.
    float m_xyRedO[2]; float m_xyGreenO[2]; float m_xyBlueO[2]; float m_xyWhiteO[2]; // For the output color space.
    float m_xyRedC[2]; float m_xyGreenC[2]; float m_xyBlueC[2]; float m_xyWhiteC[2]; float m_scaleC; // For the output container color space (if con2).
    float m_softGap; // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
    float m_hdrMax; // Maximum input value.
    float m_exposure; // Number of stops between 'hdrMax' and 18% mid-level on input.
    float m_contrast; // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
    float m_shoulderContrast; // Shoulder shaping, 1.0 = no change (fast path).
    float m_saturation[3]; // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
    float m_crosstalk[3]; // One channel must be 1.0, the rest can be <= 1.0 but not zero.
    // LPM Only

    DisplayMode m_displayMode;
    float4x4 m_inputToOutputMatrix;
};