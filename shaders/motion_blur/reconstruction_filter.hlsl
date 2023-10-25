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

#define MOTION_BLUR_DEBUG 0

#if MOTION_BLUR_DEBUG
#define OUTPUT_COLOR(color, debugColor) color * debugColor
#else
#define OUTPUT_COLOR(color, debugColor) color
#endif

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 randomValue = float2(InterleavedGradientNoise(dispatchThreadID, 0), InterleavedGradientNoise(dispatchThreadID, 1)) - 0.5;

    float2 tilePos = (0.5f + dispatchThreadID) / MOTION_BLUR_TILE_SIZE;    
    float2 tileTextureSize = (SceneCB.displaySize + MOTION_BLUR_TILE_SIZE - 1) / MOTION_BLUR_TILE_SIZE;
    float2 tileUV = (tilePos + randomValue) / tileTextureSize;
    
    float3 neighborVelocity = neighborMaxTexture.SampleLevel(pointSampler, tileUV, 0).xyz;

    float tileVelocityLengthMax = neighborVelocity.x;
    float tileVelocityLengthMin = neighborVelocity.z;
    
    bool earlyExit = neighborVelocity.x <= 0.5;
    bool fastPath = tileVelocityLengthMin > 0.6 * tileVelocityLengthMax;
    
    if (WaveActiveAllTrue(earlyExit))
    {
        outputTexture[dispatchThreadID] = OUTPUT_COLOR(colorTexture[dispatchThreadID], float4(0, 0, 1, 1));
        return;
    }
    
    float2 tileVelocity = DecodeVelocity(neighborVelocity.xy);
    float2 centerUV = GetScreenUV(dispatchThreadID.xy, SceneCB.rcpDisplaySize);
    float3 centerColor = colorTexture[dispatchThreadID].xyz;
    
    uint stepCount = c_sampleCount / 2;
    
    if (WaveActiveAllTrue(fastPath))
    {
        float3 sum = centerColor;
        
        for (int i = 1; i <= stepCount; ++i)
        {
            float offset0 = float(i + randomValue.x) / c_sampleCount;
            float offset1 = float(-i + randomValue.x) / c_sampleCount;
        
            float2 sampleUV0 = centerUV + tileVelocity * float2(0.5, -0.5) * offset0;
            float2 sampleUV1 = centerUV + tileVelocity * float2(0.5, -0.5) * offset1;
            
            sum += colorTexture.SampleLevel(pointSampler, sampleUV0, 0.0).xyz;
            sum += colorTexture.SampleLevel(pointSampler, sampleUV1, 0.0).xyz;
        }
        
        sum *= rcp(stepCount * 2.0 + 1.0);
        
        outputTexture[dispatchThreadID] = OUTPUT_COLOR(float4(sum, 1), float4(0, 1, 0, 1));
    }
    else
    {
        float centerVelocity = velocityDepthTexture[dispatchThreadID].x;
        float centerDepth = velocityDepthTexture[dispatchThreadID].z;
        
        float4 sum = 0.0;
    
        for (int i = 1; i <= stepCount; ++i)
        {
            float offset0 = float(i + randomValue.x) / c_sampleCount;
            float offset1 = float(-i + randomValue.x) / c_sampleCount;
        
            float2 sampleUV0 = centerUV + tileVelocity * float2(0.5, -0.5) * offset0;
            float2 sampleUV1 = centerUV + tileVelocity * float2(0.5, -0.5) * offset1;
        
            float4 sampleVelocityDepth0 = velocityDepthTexture.SampleLevel(pointSampler, sampleUV0, 0.0);
            float sampleDepth0 = sampleVelocityDepth0.z;
            float sampleVelocity0 = sampleVelocityDepth0.x;
        
            float4 sampleVelocityDepth1 = velocityDepthTexture.SampleLevel(pointSampler, sampleUV1, 0.0);
            float sampleDepth1 = sampleVelocityDepth1.z;
            float sampleVelocity1 = sampleVelocityDepth1.x;
        
            float offsetLength0 = length((sampleUV0 - centerUV) * SceneCB.displaySize);
            float offsetLength1 = length((sampleUV1 - centerUV) * SceneCB.displaySize);
        
            float weight0 = SampleWeight(centerDepth, sampleDepth0, offsetLength0, centerVelocity, sampleVelocity0, 1.0, MOTION_BLUR_SOFT_DEPTH_EXTENT);
            float weight1 = SampleWeight(centerDepth, sampleDepth1, offsetLength1, centerVelocity, sampleVelocity1, 1.0, MOTION_BLUR_SOFT_DEPTH_EXTENT);
        
            bool2 mirror = bool2(sampleDepth0 > sampleDepth1, sampleVelocity1 > sampleVelocity0);
            weight0 = all(mirror) ? weight1 : weight0;
            weight1 = any(mirror) ? weight1 : weight0;
        
            sum += weight0 * float4(colorTexture.SampleLevel(pointSampler, sampleUV0, 0.0).xyz, 1.0);
            sum += weight1 * float4(colorTexture.SampleLevel(pointSampler, sampleUV1, 0.0).xyz, 1.0);
        }
    
        sum *= rcp(stepCount * 2.0 + 1.0);
        float4 result = float4(sum.rgb + (1.0 - sum.w) * centerColor, 1);
        
        outputTexture[dispatchThreadID] = OUTPUT_COLOR(result, float4(1, 0, 0, 1));
    }
}
