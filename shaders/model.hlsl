#include "model_constants.hlsli"

cbuffer VertexCB : register(b0)
{
    uint c_posBuffer;
    uint c_uvBuffer;
    uint c_normalBuffer;
    uint c_tangentBuffer;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
#endif
    float3 worldPos : TEXCOORD1;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[c_posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[c_uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[c_normalBuffer];
    StructuredBuffer<float3> tangentBuffer = ResourceDescriptorHeap[c_tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    
    VSOutput output;
    output.pos = mul(ModelCB.mtxWVP, pos);
    output.uv = uvBuffer[vertex_id];
    output.normal = mul(ModelCB.mtxNormal, float4(normalBuffer[vertex_id], 0.0f)).xyz;
    output.worldPos = mul(ModelCB.mtxWorld, pos).xyz;
    
#if NORMAL_TEXTURE
    output.tangent = mul(ModelCB.mtxWorld, float4(tangentBuffer[vertex_id], 0.0f)).xyz;
#endif
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    return float4(input.uv, 0.0, 1.0);
}
