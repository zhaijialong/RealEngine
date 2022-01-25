#include "common.hlsli"

cbuffer RaytraceConstants : register(b0)
{
    uint c_depthSRV;
    uint c_normalSRV;
    uint c_shadowUAV;
};

[numthreads(8, 8, 1)]
void raytrace_shadow(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= SceneCB.viewWidth || dispatchThreadID.y >= SceneCB.viewHeight)
    {
        return;
    }
    
    int2 pos = dispatchThreadID.xy;
    
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthSRV];
    float depth = depthRT[pos];
    if (depth == 0.0)
    {
        return;
    }

    float3 worldPos = GetWorldPosition(pos, depth);

    Texture2D normalRT = ResourceDescriptorHeap[c_normalSRV];
    float3 N = OctNormalDecode(normalRT[pos].xyz);

    RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[SceneCB.sceneRayTracingTLAS];

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = SceneCB.lightDir;
    ray.TMin = 0.0;
    ray.TMax = 1000.0;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

    q.TraceRayInline(
        raytracingAS,
        RAY_FLAG_NONE,
        0xFF,
        ray);

    q.Proceed();
    
    float visibility = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;

    RWTexture2D<unorm float> shadowRT = ResourceDescriptorHeap[c_shadowUAV];
    shadowRT[pos] = visibility;
}