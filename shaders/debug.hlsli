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

void DrawDebugTriangle(float3 p0, float3 p1, float3 p2, float3 color)
{
    DrawDebugLine(p0, p1, color);
    DrawDebugLine(p1, p2, color);
    DrawDebugLine(p2, p0, color);
}

void DrawDebugBox(float3 min, float3 max, float3 color)
{
    float3 p0 = float3(min.x, min.y, min.z);
    float3 p1 = float3(max.x, min.y, min.z);
    float3 p2 = float3(max.x, max.y, min.z);
    float3 p3 = float3(min.x, max.y, min.z);
    float3 p4 = float3(min.x, min.y, max.z);
    float3 p5 = float3(max.x, min.y, max.z);
    float3 p6 = float3(max.x, max.y, max.z);
    float3 p7 = float3(min.x, max.y, max.z);

    DrawDebugLine(p0, p1, color);
    DrawDebugLine(p1, p2, color);
    DrawDebugLine(p2, p3, color);
    DrawDebugLine(p3, p0, color);

    DrawDebugLine(p4, p5, color);
    DrawDebugLine(p5, p6, color);
    DrawDebugLine(p6, p7, color);
    DrawDebugLine(p7, p4, color);
    
    DrawDebugLine(p0, p4, color);
    DrawDebugLine(p1, p5, color);
    DrawDebugLine(p2, p6, color);
    DrawDebugLine(p3, p7, color);
}

static const uint DEBUG_SPHERE_M = 10; //latitude (horizontal)
static const uint DEBUG_SPHERE_N = 20; //longitude (vertical)

float3 DebugSpherePosition(uint m, uint n, float3 center, float radius)
{
    float x = sin(M_PI * m / DEBUG_SPHERE_M) * cos(2 * M_PI * n / DEBUG_SPHERE_N);
    float z = sin(M_PI * m / DEBUG_SPHERE_M) * sin(2 * M_PI * n / DEBUG_SPHERE_N);
    float y = cos(M_PI * m / DEBUG_SPHERE_M);
    return center + float3(x, y, z) * radius;
}

void DrawDebugSphere(float3 center, float radius, float3 color)
{
    for (uint m = 0; m < DEBUG_SPHERE_M; ++m)
    {
        for (uint n = 0; n < DEBUG_SPHERE_N; ++n)
        {
            float3 p0 = DebugSpherePosition(m, n, center, radius);
            float3 p1 = DebugSpherePosition(min(m + 1, DEBUG_SPHERE_M), n, center, radius);
            float3 p2 = DebugSpherePosition(m, min(n + 1, DEBUG_SPHERE_N), center, radius);
            
            DrawDebugTriangle(p0, p1, p2, color);
        }
    }
}