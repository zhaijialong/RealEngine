#define FXAA_PC 1
#define FXAA_HLSL_5 1
#define FXAA_QUALITY__PRESET 39
#define FXAA_GREEN_AS_LUMA 1

#include "fxaa.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    output.pos.x = (float) (vertex_id / 2) * 4.0 - 1.0;
    output.pos.y = (float) (vertex_id % 2) * 4.0 - 1.0;
    output.pos.z = 0.0;
    output.pos.w = 1.0;
    output.uv.x = (float) (vertex_id / 2) * 2.0;
    output.uv.y = 1.0 - (float) (vertex_id % 2) * 2.0;

    return output;
}

cbuffer CB : register(b0)
{
    uint c_ldrTexture;
    uint c_linearSampler;
    float c_rcpWidth;
    float c_rcpHeight;
};

float4 ps_main(VSOutput input) : SV_TARGET
{
    Texture2D ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[c_linearSampler];
    
    FxaaTex tex = { linearSampler, ldrTexture };
    FxaaFloat2 pos = input.uv;
    FxaaFloat2 fxaaQualityRcpFrame = FxaaFloat2(c_rcpWidth, c_rcpHeight);
    FxaaFloat fxaaQualitySubpix = 0.75;
    FxaaFloat fxaaQualityEdgeThreshold = 0.125;
    FxaaFloat fxaaQualityEdgeThresholdMin = 0.0833;
    
    // not used
    FxaaFloat4 fxaaConsolePosPos;
    FxaaFloat4 fxaaConsoleRcpFrameOpt;
    FxaaFloat4 fxaaConsoleRcpFrameOpt2;
    FxaaFloat4 fxaaConsole360RcpFrameOpt2;
    FxaaFloat fxaaConsoleEdgeSharpness;
    FxaaFloat fxaaConsoleEdgeThreshold;
    FxaaFloat fxaaConsoleEdgeThresholdMin;
    FxaaFloat4 fxaaConsole360ConstDir;
    
    return FxaaPixelShader(
        pos, 
        fxaaConsolePosPos, 
        tex,
        tex,
        tex, 
        fxaaQualityRcpFrame, 
        fxaaConsoleRcpFrameOpt, 
        fxaaConsoleRcpFrameOpt2, 
        fxaaConsole360RcpFrameOpt2,
        fxaaQualitySubpix, 
        fxaaQualityEdgeThreshold, 
        fxaaQualityEdgeThresholdMin, 
        fxaaConsoleEdgeSharpness, 
        fxaaConsoleEdgeThreshold, 
        fxaaConsoleEdgeThresholdMin, 
        fxaaConsole360ConstDir);
}