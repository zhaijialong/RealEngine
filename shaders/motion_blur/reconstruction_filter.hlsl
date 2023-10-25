#include "motion_blur_common.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b0)
{
    uint c_colorTexture;
    uint c_velocityDepthTexture;
    uint c_neighborMaxTexture;
    uint c_outputTexture;

    uint c_sampleCount;
};

static Texture2D colorTexture = ResourceDescriptorHeap[c_colorTexture];
static Texture2D velocityDepthTexture = ResourceDescriptorHeap[c_velocityDepthTexture];
static Texture2D neighborMaxTexture = ResourceDescriptorHeap[c_neighborMaxTexture];
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

float GetVelocityLength(float2 v)
{
    const float exposureTime = GetCameraCB().physicalCamera.shutterSpeed;
    const float frameRate = 1.0 / SceneCB.frameTime;
    
    return length(v * 0.5 * float2(SceneCB.displaySize) * exposureTime * frameRate);
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 randomValue = float2(InterleavedGradientNoise(dispatchThreadID, 0), InterleavedGradientNoise(dispatchThreadID, 1)) - 0.5;

    float2 tilePos = (0.5f + dispatchThreadID) / MOTION_BLUR_TILE_SIZE;    
    float2 tileTextureSize = (SceneCB.displaySize + MOTION_BLUR_TILE_SIZE - 1) / MOTION_BLUR_TILE_SIZE;
    float2 tileUV = (tilePos + randomValue) / tileTextureSize;
    
    float2 maxNeighborVelocity = neighborMaxTexture.SampleLevel(pointSampler, tileUV, 0).xy;
    
    if (GetVelocityLength(maxNeighborVelocity) <= 0.5)
    {
        outputTexture[dispatchThreadID] = colorTexture[dispatchThreadID];
        return;
    }
    
    float2 centerUV = GetScreenUV(dispatchThreadID.xy, SceneCB.rcpDisplaySize);
    float3 centerColor = colorTexture[dispatchThreadID].xyz;
    float centerVelocity = GetVelocityLength(velocityDepthTexture[dispatchThreadID].xy * 2.0 - 1.0);
    float centerDepth = velocityDepthTexture[dispatchThreadID].z;
        
    float sampleCount = 0.0;
    float4 sum = 0.0;
    
    for (int i = 1; i <= c_sampleCount / 2; ++i)
    {        
        float offset0 = float(i + randomValue.x) / c_sampleCount;
        float offset1 = float(-i + randomValue.x) / c_sampleCount;
        
        float2 sampleUV0 = centerUV + maxNeighborVelocity * float2(0.5, -0.5) * offset0;
        float2 sampleUV1 = centerUV + maxNeighborVelocity * float2(0.5, -0.5) * offset1;
        
        float4 sampleVelocityDepth0 = velocityDepthTexture.SampleLevel(pointSampler, sampleUV0, 0.0);
        float sampleDepth0 = sampleVelocityDepth0.z;
        float sampleVelocity0 = GetVelocityLength(sampleVelocityDepth0.xy * 2.0 - 1.0);
        
        float4 sampleVelocityDepth1 = velocityDepthTexture.SampleLevel(pointSampler, sampleUV1, 0.0);
        float sampleDepth1 = sampleVelocityDepth1.z;
        float sampleVelocity1 = GetVelocityLength(sampleVelocityDepth1.xy * 2.0 - 1.0);
        
        float offsetLength0 = length((sampleUV0 - centerUV) * SceneCB.displaySize);
        float offsetLength1 = length((sampleUV1 - centerUV) * SceneCB.displaySize);
        
        float weight0 = SampleWeight(centerDepth, sampleDepth0, offsetLength0, centerVelocity, sampleVelocity0, 1.0, MOTION_BLUR_SOFT_DEPTH_EXTENT);
        float weight1 = SampleWeight(centerDepth, sampleDepth1, offsetLength1, centerVelocity, sampleVelocity1, 1.0, MOTION_BLUR_SOFT_DEPTH_EXTENT);
        
        bool2 mirror = bool2(sampleDepth0 > sampleDepth1, sampleVelocity1 > sampleVelocity0);
        weight0 = all(mirror) ? weight1 : weight0;
        weight1 = any(mirror) ? weight1 : weight0;
        
        sampleCount += 2.0;
        sum += weight0 * float4(colorTexture.SampleLevel(pointSampler, sampleUV0, 0.0).xyz, 1.0);
        sum += weight1 * float4(colorTexture.SampleLevel(pointSampler, sampleUV1, 0.0).xyz, 1.0);
    }
    
    sum *= rcp(sampleCount);
    
    outputTexture[dispatchThreadID] = float4(sum.rgb + (1.0 - sum.w) * centerColor, 1);
}
