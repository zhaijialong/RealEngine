#include "common.hlsli"
#include "ray_trace.hlsli"

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
    
    float visibility = rt::VisibilityRay(worldPos + N * 0.01, SceneCB.lightDir, 1000.0);

    RWTexture2D<unorm float> shadowRT = ResourceDescriptorHeap[c_shadowUAV];
    shadowRT[pos] = visibility;
}