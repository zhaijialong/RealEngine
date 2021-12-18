#pragma once

#include "common.hlsli"
#include "model_constants.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
#endif
    
    uint meshlet : COLOR;
};

VertexOut GetVertex(uint vertex_id)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
    StructuredBuffer<float4> tangentBuffer = ResourceDescriptorHeap[ModelCB.tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    float4 worldPos = mul(ModelCB.mtxWorld, pos);

    VertexOut v = (VertexOut)0;
    v.pos = mul(CameraCB.mtxViewProjection, worldPos);
    v.uv = uvBuffer[vertex_id];
    v.normal = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(normalBuffer[vertex_id], 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //debug::DrawSphere(ModelCB.center, ModelCB.radius, float3(1, 0, 0));
    }
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    float4 tangent = tangentBuffer[vertex_id];
    v.tangent = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(tangent.xyz, 0.0f)).xyz);
    v.bitangent = normalize(cross(v.normal, v.tangent) * tangent.w);    
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.tangent * 0.05, float3(1, 0, 0));
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.bitangent * 0.05, float3(0, 1, 0));
#endif
    
    return v;
}