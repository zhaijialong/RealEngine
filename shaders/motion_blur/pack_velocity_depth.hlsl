#include "motion_blur_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_velocityTexture;
    uint c_depthTexture;
    uint c_outputTexture;
};

static Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
static Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
static RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 uv = GetScreenUV(dispatchThreadID, SceneCB.rcpDisplaySize);
    
    //todo : polar coordinate
    float2 velocity = velocityTexture.SampleLevel(pointSampler, uv, 0).xy * 0.5 + 0.5; //[-1,1] -> [0,1]

    float depth = depthTexture.SampleLevel(pointSampler, uv, 0).x;
    depth = depth > 0.0 ? GetLinearDepth(depth) : 65500.0;
    
    outputTexture[dispatchThreadID] = float3(velocity, depth);
}