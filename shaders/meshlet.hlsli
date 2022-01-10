#pragma once

struct Meshlet
{
    float3 center;
    float radius;
    
    uint cone; //axis + cutoff, rgba8snorm
    
    uint vertexCount;
    uint triangleCount;
    
    uint vertexOffset;
    uint triangleOffset;
};

struct MeshletPayload
{
    uint instanceIndices[32];
    uint meshletIndices[32];
};