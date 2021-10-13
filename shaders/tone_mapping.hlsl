cbuffer ResourceCB : register(b0)
{
    uint c_hdrTexture;
    uint c_ldrTexture;
    uint c_width;
    uint c_height;
};

cbuffer cbPerFrame : register(b1)
{
    bool u_shoulder;
    bool u_con;
    bool u_soft;
    bool u_con2;
    bool u_clip;
    bool u_scaleOnly;
    uint u_displayMode;
    uint pad;
    matrix u_inputToOutputMatrix;
    uint4 u_ctl[24];
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}


#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

uint4 LpmFilterCtl(uint i)
{
    return u_ctl[i];
}

#define LPM_NO_SETUP 1
#include "ffx_lpm.h"

#include "common.hlsli"

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_width || dispatchThreadID.y >= c_height)
    {
        return;
    }
    
    RWTexture2D<float4> ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];
    
    float4 color = hdrTexture[dispatchThreadID.xy];
    //color.xyz = ACESFilm(color.xyz);
    
    color = mul(u_inputToOutputMatrix, color);
    
    LpmFilter(color.r, color.g, color.b, u_shoulder, u_con, u_soft, u_con2, u_clip, u_scaleOnly);

    /*
    switch (u_displayMode)
    {
        case 1:
            // FS2_DisplayNative
            // Apply gamma
            color.xyz = ApplyGamma(color.xyz);
            break;

        case 3:
            // HDR10_ST2084
            // Apply ST2084 curve
            color.xyz = ApplyPQ(color.xyz);
            break;
    }
    */
    
    ldrTexture[dispatchThreadID.xy] = float4(LinearToSrgb(color.xyz), 1.0);
}