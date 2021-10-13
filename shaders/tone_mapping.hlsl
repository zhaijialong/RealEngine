cbuffer ResourceCB : register(b0)
{
    uint c_hdrTexture;
    uint c_pointSampler;
    uint c_padding0;
    uint c_padding1;
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

#include "transferFunction.h"

float4 ps_main(VSOutput input) : SV_TARGET
{
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[c_pointSampler];
    
    float4 color = hdrTexture.Sample(pointSampler, input.uv);
    //color.xyz = ACESFilm(color.xyz);
    
    color = mul(u_inputToOutputMatrix, color);
    
    LpmFilter(color.r, color.g, color.b, u_shoulder, u_con, u_soft, u_con2, u_clip, u_scaleOnly);

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
    
    return color;
}