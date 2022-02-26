#include "ray_trace.hlsli"
#include "random.hlsli"

cbuffer PathTracingConstants : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    uint c_depthTRT;

    uint c_outputTexture;
};

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 getCosHemisphereSample(inout PRNG random, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
    float2 randVal = float2(random.RandomFloat(), random.RandomFloat());

	// Cosine weighted hemisphere sample from RNG
    float3 bitangent = getPerpendicularVector(hitNorm);
    float3 tangent = cross(bitangent, hitNorm);
    float r = sqrt(randVal.x);
    float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}


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
    
    PRNG random = PRNG::Create(dispatchThreadID.x + dispatchThreadID.y * SceneCB.viewWidth, SceneCB.frameIndex);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = getCosHemisphereSample(random, N);
    ray.TMin = 0.001;
    ray.TMax = 1.0;

    float visibility = rt::TraceVisibilityRay(ray) ? 3.0 : 0.0;

    float3 radiance = float3(visibility, visibility, visibility);

    outputTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}

cbuffer AccumulationConstants : register(b0)
{
    uint c_currentFrameTexture;
    uint c_historyTexture;
    uint c_accumulationTexture;
};

[numthreads(8, 8, 1)]
void accumulation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D currentFrameTexture = ResourceDescriptorHeap[c_currentFrameTexture];
    RWTexture2D<float4> historyTexture = ResourceDescriptorHeap[c_historyTexture];
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[c_accumulationTexture];

    float3 current = currentFrameTexture[dispatchThreadID.xy].xyz;
    float3 history = historyTexture[dispatchThreadID.xy].xyz;

    float3 accumulation = (SceneCB.frameIndex * history + current) / (SceneCB.frameIndex + 1);

    historyTexture[dispatchThreadID.xy] = float4(accumulation, 1.0);
    accumulationTexture[dispatchThreadID.xy] = float4(accumulation, 1.0);
}