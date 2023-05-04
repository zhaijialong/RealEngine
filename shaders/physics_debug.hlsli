#pragma once

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
    uint color;
};

struct Constants
{    
    float4x4 mtxWorld;
    float4x4 mtxWorldInverseTranspose;

    uint vertexBuffer;
    uint color;
};

#ifdef __cplusplus
static_assert(sizeof(Vertex) == sizeof(JPH::DebugRenderer::Vertex));
#endif