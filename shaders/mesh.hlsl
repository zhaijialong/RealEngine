#include "common.hlsli"

cbuffer cb : register(b1)
{
    float4x4 c_mtxVP;
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
    output.pos = mul(c_mtxVP, output.pos);
    output.uv.x = (float) (vertex_id / 2) * 2.0;
    output.uv.y = 1.0 - (float) (vertex_id % 2) * 2.0;
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    return float4(input.uv, 0.0, 1.0);
}