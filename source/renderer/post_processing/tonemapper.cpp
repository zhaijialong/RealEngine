#include "tonemapper.h"
#include "../renderer.h"

// LPM
#define A_CPU 1
#include "ffx_a.h"
A_STATIC AF1 fs2S;
A_STATIC AF1 hdr10S;
A_STATIC AU1 ctl[24 * 4];

A_STATIC void LpmSetupOut(AU1 i, inAU4 v)
{
    for (int j = 0; j < 4; ++j) { ctl[i * 4 + j] = v[j]; }
}
#include "ffx_lpm.h"

float4x4 CalculateRGBToXYZMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag)
{
    float Xw = xw / yw;
    float Yw = 1;
    float Zw = (1 - xw - yw) / yw;

    float Xr = xr / yr;
    float Yr = 1;
    float Zr = (1 - xr - yr) / yr;

    float Xg = xg / yg;
    float Yg = 1;
    float Zg = (1 - xg - yg) / yg;

    float Xb = xb / yb;
    float Yb = 1;
    float Zb = (1 - xb - yb) / yb;

    float4x4 XRGB = float4x4(
        float4(Xr, Xg, Xb, 0),
        float4(Yr, Yg, Yb, 0),
        float4(Zr, Zg, Zb, 0),
        float4(0, 0, 0, 1));
    float4x4 XRGBInverse = inverse(XRGB);

    float4 referenceWhite = float4(Xw, Yw, Zw, 0);
    float4 SRGB = mul(transpose(XRGBInverse), referenceWhite);

    float4x4 RegularResult = float4x4(
        float4(SRGB.x * Xr, SRGB.y * Xg, SRGB.z * Xb, 0),
        float4(SRGB.x * Yr, SRGB.y * Yg, SRGB.z * Yb, 0),
        float4(SRGB.x * Zr, SRGB.y * Zg, SRGB.z * Zb, 0),
        float4(0, 0, 0, 1));

    if (!scaleLumaFlag)
        return RegularResult;

    float4x4 Scale = scaling_matrix(float3(100.0f, 100.0f, 100.0f));
    float4x4 Result = mul(RegularResult, Scale);
    return Result;
}

float4x4 CalculateXYZToRGBMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag)
{
    auto RGBToXYZ = CalculateRGBToXYZMatrix(xw, yw, xr, yr, xg, yg, xb, yb, scaleLumaFlag);
    return inverse(RGBToXYZ);
}

enum ColorPrimaries
{
    ColorPrimaries_WHITE,
    ColorPrimaries_RED,
    ColorPrimaries_GREEN,
    ColorPrimaries_BLUE
};

enum ColorPrimariesCoordinates
{
    ColorPrimariesCoordinates_X,
    ColorPrimariesCoordinates_Y
};

float ColorSpacePrimaries[4][4][2] = {
    //Rec709
    {
        0.3127f,0.3290f, // White point
        0.64f,0.33f, // Red point
        0.30f,0.60f, // Green point
        0.15f,0.06f, // Blue point
    },
    //P3
    {
        0.3127f,0.3290f, // White point
        0.680f,0.320f, // Red point
        0.265f,0.690f, // Green point
        0.150f,0.060f, // Blue point
    },
    //Rec2020
    {
        0.3127f,0.3290f, // White point
        0.708f,0.292f, // Red point
        0.170f,0.797f, // Green point
        0.131f,0.046f, // Blue point
    },
    //Display Specific zeroed out now Please fill them in once you query them and want to use them for matrix calculations
    {
        0.0f,0.0f, // White point
        0.0f,0.0f, // Red point
        0.0f,0.0f, // Green point
        0.0f,0.0f // Blue point
    }
};

void SetupGamutMapperMatrices(ColorSpace gamutIn, ColorSpace gamutOut, float4x4* inputToOutputRecMatrix)
{
    float4x4 intputGamut_To_XYZ = CalculateRGBToXYZMatrix(
        ColorSpacePrimaries[gamutIn][ColorPrimaries_WHITE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_WHITE][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutIn][ColorPrimaries_RED][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_RED][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutIn][ColorPrimaries_GREEN][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_GREEN][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutIn][ColorPrimaries_BLUE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_BLUE][ColorPrimariesCoordinates_Y],
        false);

    float4x4 XYZ_To_OutputGamut = CalculateXYZToRGBMatrix(
        ColorSpacePrimaries[gamutOut][ColorPrimaries_WHITE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_WHITE][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutOut][ColorPrimaries_RED][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_RED][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutOut][ColorPrimaries_GREEN][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_GREEN][ColorPrimariesCoordinates_Y],
        ColorSpacePrimaries[gamutOut][ColorPrimaries_BLUE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_BLUE][ColorPrimariesCoordinates_Y],
        false);

    float4x4 intputGamut_To_OutputGamut = mul(intputGamut_To_XYZ, XYZ_To_OutputGamut);
    *inputToOutputRecMatrix = transpose(intputGamut_To_OutputGamut);
}

Tonemapper::Tonemapper(Renderer* pRenderer, DisplayMode display_mode, ColorSpace color_space)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("tone_mapping.hlsl", "cs_main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "LPM PSO");

    m_displayMode = display_mode;

    SetupGamutMapperMatrices(
        ColorSpace_REC709,
        color_space,
        &m_inputToOutputMatrix
    );

    varAF2(fs2R);
    varAF2(fs2G);
    varAF2(fs2B);
    varAF2(fs2W);
    varAF2(displayMinMaxLuminance);
    if (display_mode != DISPLAYMODE_SDR)
    {
        //TODO
        /*
        const DXGI_OUTPUT_DESC1* displayInfo = GetDisplayInfo();

        // Only used in fs2 modes
        fs2R[0] = displayInfo->RedPrimary[0];
        fs2R[1] = displayInfo->RedPrimary[1];
        fs2G[0] = displayInfo->GreenPrimary[0];
        fs2G[1] = displayInfo->GreenPrimary[1];
        fs2B[0] = displayInfo->BluePrimary[0];
        fs2B[1] = displayInfo->BluePrimary[1];
        fs2W[0] = displayInfo->WhitePoint[0];
        fs2W[1] = displayInfo->WhitePoint[1];
        // Only used in fs2 modes

        displayMinMaxLuminance[0] = displayInfo->MinLuminance;
        displayMinMaxLuminance[1] = displayInfo->MaxLuminance;
        */
    }
    
    //TODO : UI
    m_shoulder = true;
    m_softGap = 0.0f;
    m_hdrMax = 8.0f;
    m_exposure = 3.0f;
    m_contrast = 0.3f;
    m_shoulderContrast = 1.0f;
    m_saturation[0] = 0.0f; 
    m_saturation[1] = 0.0f;
    m_saturation[2] = 0.0f;
    m_crosstalk[0] = 1.0f; 
    m_crosstalk[1] = 0.5f; 
    m_crosstalk[2] = 0.3125f;
    
    switch (color_space)
    {
    case ColorSpace_REC709:
    {
        switch (display_mode)
        {
        case DISPLAYMODE_SDR:
            SetLPMConfig(LPM_CONFIG_709_709);
            SetLPMColors(LPM_COLORS_709_709);
            break;

        case DISPLAYMODE_FSHDR_Gamma22:
            SetLPMConfig(LPM_CONFIG_FS2RAW_709);
            SetLPMColors(LPM_COLORS_FS2RAW_709);
            break;

        case DISPLAYMODE_FSHDR_SCRGB:
            fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_FS2SCRGB_709);
            SetLPMColors(LPM_COLORS_FS2SCRGB_709);
            break;

        case DISPLAYMODE_HDR10_2084:
            hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10RAW_709);
            SetLPMColors(LPM_COLORS_HDR10RAW_709);
            break;

        case DISPLAYMODE_HDR10_SCRGB:
            hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10SCRGB_709);
            SetLPMColors(LPM_COLORS_HDR10SCRGB_709);
            break;

        default:
            break;
        }
        break;
    }

    case ColorSpace_P3:
    {
        switch (display_mode)
        {
        case DISPLAYMODE_SDR:
            SetLPMConfig(LPM_CONFIG_709_P3);
            SetLPMColors(LPM_COLORS_709_P3);
            break;

        case DISPLAYMODE_FSHDR_Gamma22:
            SetLPMConfig(LPM_CONFIG_FS2RAW_P3);
            SetLPMColors(LPM_COLORS_FS2RAW_P3);
            break;

        case DISPLAYMODE_FSHDR_SCRGB:
            fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_FS2SCRGB_P3);
            SetLPMColors(LPM_COLORS_FS2SCRGB_P3);
            break;

        case DISPLAYMODE_HDR10_2084:
            hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10RAW_P3);
            SetLPMColors(LPM_COLORS_HDR10RAW_P3);
            break;

        case DISPLAYMODE_HDR10_SCRGB:
            hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10SCRGB_P3);
            SetLPMColors(LPM_COLORS_HDR10SCRGB_P3);
            break;

        default:
            break;
        }
        break;
    }

    case ColorSpace_REC2020:
    {
        switch (display_mode)
        {
        case DISPLAYMODE_SDR:
            SetLPMConfig(LPM_CONFIG_709_2020);
            SetLPMColors(LPM_COLORS_709_2020);
            break;

        case DISPLAYMODE_FSHDR_Gamma22:
            SetLPMConfig(LPM_CONFIG_FS2RAW_2020);
            SetLPMColors(LPM_COLORS_FS2RAW_2020);
            break;

        case DISPLAYMODE_FSHDR_SCRGB:
            fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_FS2SCRGB_2020);
            SetLPMColors(LPM_COLORS_FS2SCRGB_2020);
            break;

        case DISPLAYMODE_HDR10_2084:
            hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10RAW_2020);
            SetLPMColors(LPM_COLORS_HDR10RAW_2020);
            break;

        case DISPLAYMODE_HDR10_SCRGB:
            hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
            SetLPMConfig(LPM_CONFIG_HDR10SCRGB_2020);
            SetLPMColors(LPM_COLORS_HDR10SCRGB_2020);
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    LpmSetup(m_shoulder, m_con, m_soft, m_con2, m_clip, m_scaleOnly,
        m_xyRedW, m_xyGreenW, m_xyBlueW, m_xyWhiteW,
        m_xyRedO, m_xyGreenO, m_xyBlueO, m_xyWhiteO,
        m_xyRedC, m_xyGreenC, m_xyBlueC, m_xyWhiteC,
        m_scaleC,
        m_softGap, m_hdrMax, m_exposure, m_contrast, m_shoulderContrast,
        m_saturation, m_crosstalk);
}

void Tonemapper::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrSRV, IGfxDescriptor* pLdrUAV, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    uint32_t resourceCB[4] = { pHdrSRV->GetHeapIndex(), pLdrUAV->GetHeapIndex(), width, height };
    pCommandList->SetComputeConstants(0, resourceCB, sizeof(resourceCB));

    struct LPMConsts {
        uint32_t shoulder;
        uint32_t con;
        uint32_t soft;
        uint32_t con2;
        uint32_t clip;
        uint32_t scaleOnly;
        uint32_t displayMode;
        uint32_t pad;
        float4x4 inputToOutputMatrix;
        uint32_t ctl[24 * 4];
    };

    LPMConsts lpmConsts;
    lpmConsts.shoulder = m_shoulder;
    lpmConsts.con = m_con;
    lpmConsts.soft = m_soft;
    lpmConsts.con2 = m_con2;
    lpmConsts.clip = m_clip;
    lpmConsts.scaleOnly = m_scaleOnly;
    lpmConsts.displayMode = m_displayMode;
    lpmConsts.pad = 0;
    lpmConsts.inputToOutputMatrix = m_inputToOutputMatrix;
    for (int i = 0; i < 4 * 24; ++i)
    {
        lpmConsts.ctl[i] = ctl[i];
    }

    pCommandList->SetComputeConstants(1, &lpmConsts, sizeof(lpmConsts));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void Tonemapper::SetLPMConfig(bool con, bool soft, bool con2, bool clip, bool scaleOnly)
{
    m_con = con;
    m_soft = soft;
    m_con2 = con2;
    m_clip = clip;
    m_scaleOnly = scaleOnly;
}

void Tonemapper::SetLPMColors(float xyRedW[2], float xyGreenW[2], float xyBlueW[2], float xyWhiteW[2], float xyRedO[2], float xyGreenO[2], float xyBlueO[2], float xyWhiteO[2], float xyRedC[2], float xyGreenC[2], float xyBlueC[2], float xyWhiteC[2], float scaleC)
{
    m_xyRedW[0] = xyRedW[0]; m_xyRedW[1] = xyRedW[1];
    m_xyGreenW[0] = xyGreenW[0]; m_xyGreenW[1] = xyGreenW[1];
    m_xyBlueW[0] = xyBlueW[0]; m_xyBlueW[1] = xyBlueW[1];
    m_xyWhiteW[0] = xyWhiteW[0]; m_xyWhiteW[1] = xyWhiteW[1];

    m_xyRedO[0] = xyRedO[0]; m_xyRedO[1] = xyRedO[1];
    m_xyGreenO[0] = xyGreenO[0]; m_xyGreenO[1] = xyGreenO[1];
    m_xyBlueO[0] = xyBlueO[0]; m_xyBlueO[1] = xyBlueO[1];
    m_xyWhiteO[0] = xyWhiteO[0]; m_xyWhiteO[1] = xyWhiteO[1];

    m_xyRedC[0] = xyRedC[0]; m_xyRedC[1] = xyRedC[1];
    m_xyGreenC[0] = xyGreenC[0]; m_xyGreenC[1] = xyGreenC[1];
    m_xyBlueC[0] = xyBlueC[0]; m_xyBlueC[1] = xyBlueC[1];
    m_xyWhiteC[0] = xyWhiteC[0]; m_xyWhiteC[1] = xyWhiteC[1];

    m_scaleC = scaleC;
}
