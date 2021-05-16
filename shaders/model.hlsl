#include "model.hlsli"

cbuffer vertexCB : register(b0)
{
    uint c_posBuffer;
    uint padding1;
    uint padding2;
    uint padding3;
};

ConstantBuffer<ModelConstant> modelCB : register(b1);

struct VSOutput
{
    float4 pos : SV_POSITION;
//    float2 uv : TEXCOORD;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[c_posBuffer];
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    
    VSOutput output;
    output.pos = mul(modelCB.mtxWVP, pos);
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    return float4(1.0, 1.0, 0.0, 1.0);
}