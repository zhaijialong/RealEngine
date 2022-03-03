#include "ray_trace.hlsli"
#include "random.hlsli"
#include "importance_sampling.hlsli"

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

    #define SPP 1
    
    PRNG rng = PRNG::Create(dispatchThreadID.x + dispatchThreadID.y * SceneCB.viewWidth);
    BNDS<SPP> bnds = BNDS<SPP>::Create(dispatchThreadID.xy, uint2(SceneCB.viewWidth, SceneCB.viewHeight));

    float visibility = 0;

    for (uint i = 0; i < SPP; ++i)
    {
        //float2 random = rng.RandomFloat2();
        float2 random = bnds.RandomFloat2(i);

        RayDesc ray;
        ray.Origin = worldPos + N * 0.01;
        ray.Direction = SampleConeUniform(random, SceneCB.lightRadius, SceneCB.lightDir);
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;

        visibility += rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
    }

    visibility /= SPP;

    RWTexture2D<unorm float> shadowRT = ResourceDescriptorHeap[c_shadowUAV];
    shadowRT[pos] = visibility;
}