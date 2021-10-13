#define FXAA_PC 1
#define FXAA_HLSL_5 1
#define FXAA_QUALITY__PRESET 39
#define FXAA_GREEN_AS_LUMA 1

#include "fxaa.hlsli"
#include "common.hlsli"

cbuffer CB : register(b1)
{
    uint c_ldrTexture;
    uint c_outTexture;
    uint c_linearSampler;
    uint c_width;
    uint c_height;
    float c_rcpWidth;
    float c_rcpHeight;
};

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_width || dispatchThreadID.y >= c_height)
    {
        return;
    }
    
    Texture2D ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[c_linearSampler];
    
    FxaaTex tex = { linearSampler, ldrTexture };
    FxaaFloat2 pos = (dispatchThreadID.xy + float2(0.5, 0.5)) * float2(c_rcpWidth, c_rcpHeight);
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
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outTexture];
    
    float4 color = FxaaPixelShader(
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
    
    color.xyz = LinearToSrgb(color.xyz);
    
    outTexture[dispatchThreadID.xy] = color;
}