#pragma once

#include "common.hlsli"

struct DebugLineVertex
{
    float3 position;
    uint color;
};

void DrawDebugLine(float3 start, float3 end, float3 color)
{
    RWByteAddressBuffer argumentsBuffer = ResourceDescriptorHeap[SceneCB.debugLineDrawCommandUAV];
    RWStructuredBuffer<DebugLineVertex> vertexBuffer = ResourceDescriptorHeap[SceneCB.debugLineVertexBufferUAV];

    uint vertex_count;
    argumentsBuffer.InterlockedAdd(0, 2, vertex_count); //increment vertex_count by 2
    
    DebugLineVertex p;
    p.color = Float4ToRGBA8Unorm(float4(color, 1.0));
    
    p.position = start;
    vertexBuffer[vertex_count] = p;
    
    p.position = end;
    vertexBuffer[vertex_count + 1] = p;
}