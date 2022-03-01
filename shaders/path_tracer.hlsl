#include "ray_trace.hlsli"
#include "random.hlsli"
#include "importance_sampling.hlsli"

cbuffer PathTracingConstants : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    uint c_depthTRT;

    uint c_maxRayLength;
    uint c_outputTexture;
};

[numthreads(8, 8, 1)]
void path_tracing(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float3> emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthTRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float depth = depthRT[dispatchThreadID.xy];
    if (depth == 0.0)
    {
        outputTexture[dispatchThreadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float3 worldPos = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 N = OctNormalDecode(normalRT[dispatchThreadID.xy].xyz);
    float roughness = normalRT[dispatchThreadID.xy].w;
    
    //BNDS<1> rng = BNDS<1>::Create(dispatchThreadID.xy, uint2(SceneCB.viewWidth, SceneCB.viewHeight));
    PRNG rng = PRNG::Create(dispatchThreadID.x + dispatchThreadID.y * SceneCB.viewWidth);

    float3 wo = normalize(CameraCB.cameraPos - worldPos);
    float3 H = SampleGGX(rng.RandomFloat2(), roughness, N);
    float3 L = reflect(-wo, H);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = L;
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    float3 radiance = float3(0, 0, 0);

    rt::HitInfo hitInfo;
    if (rt::TraceRay(ray, hitInfo))
    {
        rt::MaterialData material = rt::GetMaterial(hitInfo);

        radiance = material.diffuse;
    }

    outputTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}

cbuffer AccumulationConstants : register(b0)
{
    uint c_currentFrameTexture;
    uint c_historyTexture;
    uint c_accumulationTexture;
    uint c_bEnableAccumulation;
};

[numthreads(8, 8, 1)]
void accumulation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D currentFrameTexture = ResourceDescriptorHeap[c_currentFrameTexture];
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[c_accumulationTexture];

    float3 current = currentFrameTexture[dispatchThreadID.xy].xyz;
    float3 output = current;

    if (c_bEnableAccumulation)
    {
        RWTexture2D<float4> historyTexture = ResourceDescriptorHeap[c_historyTexture];
        float3 history = historyTexture[dispatchThreadID.xy].xyz;

        output = (SceneCB.frameIndex * history + current) / (SceneCB.frameIndex + 1);
        historyTexture[dispatchThreadID.xy] = float4(output, 1.0);
    }

    accumulationTexture[dispatchThreadID.xy] = float4(output, 1.0);
}