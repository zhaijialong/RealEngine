#include "hsr_common.hlsli"

cbuffer Constants : register(b1)
{
    uint c_tileListBuffer;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_velocityTexture;

    uint c_inputRadianceTexture;
    uint c_historyRadianceTexture;
    uint c_historyVarianceTexture;
    uint c_historySampleNumTexture;
    
    uint c_outputRadianceUAV;
    uint c_outputVarianceUAV;
    uint c_outputAvgRadianceUAV;
    uint c_outputSampleNumUAV;
};

float FFX_DNSR_Reflections_LoadDepth(int2 pixel_coordinate)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    return depthTexture[pixel_coordinate];
}

float FFX_DNSR_Reflections_LoadLinearDepthHistory(int2 pixel_coordinate)
{
    Texture2D<float> prevDepthTexture = ResourceDescriptorHeap[SceneCB.prevSceneDepthSRV];
    return GetLinearDepth(prevDepthTexture[pixel_coordinate]);
}

float FFX_DNSR_Reflections_SampleLinearDepthHistory(float2 uv)
{
    Texture2D<float> prevDepthTexture = ResourceDescriptorHeap[SceneCB.prevSceneDepthSRV];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return GetLinearDepth(prevDepthTexture.SampleLevel(linearSampler, uv, 0));
}

float16_t3 FFX_DNSR_Reflections_LoadRadiance(int2 pixel_coordinate)
{
    Texture2D inputRadianceTexture = ResourceDescriptorHeap[c_inputRadianceTexture];
    return (float16_t3)inputRadianceTexture[pixel_coordinate].xyz;
}

float16_t3 FFX_DNSR_Reflections_LoadRadianceHistory(int2 pixel_coordinate)
{
    Texture2D historyRadianceTexture = ResourceDescriptorHeap[c_historyRadianceTexture];
    return (float16_t3) historyRadianceTexture[pixel_coordinate].xyz;;
}

float16_t3 FFX_DNSR_Reflections_SampleRadianceHistory(float2 uv)
{
    Texture2D historyRadianceTexture = ResourceDescriptorHeap[c_historyRadianceTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return (float16_t3)historyRadianceTexture.SampleLevel(linearSampler, uv, 0).xyz;
}

float16_t FFX_DNSR_Reflections_SampleNumSamplesHistory(float2 uv)
{
    Texture2D<float> historySampleNumTexture = ResourceDescriptorHeap[c_historySampleNumTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return (float16_t)historySampleNumTexture.SampleLevel(linearSampler, uv, 0);
}

float16_t3 FFX_DNSR_Reflections_LoadWorldSpaceNormal(int2 pixel_coordinate)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return (float16_t3)DecodeNormal(normalTexture[pixel_coordinate].xyz);
}

float16_t3 FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(int2 pixel_coordinate)
{
    Texture2D prevNormalTexture = ResourceDescriptorHeap[SceneCB.prevNormalSRV];
    return (float16_t3)DecodeNormal(prevNormalTexture[pixel_coordinate].xyz);
}

float16_t3 FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(float2 uv)
{
    Texture2D prevNormalTexture = ResourceDescriptorHeap[SceneCB.prevNormalSRV];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    return (float16_t3)DecodeNormal(prevNormalTexture.SampleLevel(pointSampler, uv, 0).xyz);
}

float16_t FFX_DNSR_Reflections_LoadRoughness(int2 pixel_coordinate)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return (float16_t)normalTexture[pixel_coordinate].w;
}

float16_t FFX_DNSR_Reflections_SampleRoughnessHistory(float2 uv)
{
    Texture2D prevNormalTexture = ResourceDescriptorHeap[SceneCB.prevNormalSRV];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return (float16_t)prevNormalTexture.SampleLevel(linearSampler, uv, 0).w;
}

float2 FFX_DNSR_Reflections_LoadMotionVector(int2 pixel_coordinate)
{
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
    return velocityTexture[pixel_coordinate].xy * float2(0.5, -0.5); //velocity.xy is in ndc space;
}

float16_t FFX_DNSR_Reflections_SampleVarianceHistory(float2 uv)
{
    Texture2D<float> historyVarianceTexture = ResourceDescriptorHeap[c_historyVarianceTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return (float16_t)historyVarianceTexture.SampleLevel(linearSampler, uv, 0);
}

float16_t FFX_DNSR_Reflections_LoadRayLength(int2 pixel_coordinate)
{
    Texture2D inputRadianceTexture = ResourceDescriptorHeap[c_inputRadianceTexture];
    return (float16_t)inputRadianceTexture[pixel_coordinate].w;
}

void FFX_DNSR_Reflections_StoreRadianceReprojected(int2 pixel_coordinate, float16_t3 value)
{
    RWTexture2D<float3> outputRadianceUAV = ResourceDescriptorHeap[c_outputRadianceUAV];
    outputRadianceUAV[pixel_coordinate] = value;
}

void FFX_DNSR_Reflections_StoreAverageRadiance(int2 pixel_coordinate, float16_t3 value)
{
    RWTexture2D<float3> outputAvgRadianceUAV = ResourceDescriptorHeap[c_outputAvgRadianceUAV];
    outputAvgRadianceUAV[pixel_coordinate] = value;
}

void FFX_DNSR_Reflections_StoreVariance(int2 pixel_coordinate, float16_t value)
{
    RWTexture2D<float> outputVarianceUAV = ResourceDescriptorHeap[c_outputVarianceUAV];
    outputVarianceUAV[pixel_coordinate] = value;
}

void FFX_DNSR_Reflections_StoreNumSamples(int2 pixel_coordinate, float16_t value)
{
    RWTexture2D<float> outputSampleNumUAV = ResourceDescriptorHeap[c_outputSampleNumUAV];
    outputSampleNumUAV[pixel_coordinate] = value;
}

#include "ffx-reflection-dnsr/ffx_denoiser_reflections_reproject.h"

[numthreads(8, 8, 1)]
void main(int2 group_thread_id : SV_GroupThreadID, uint group_index : SV_GroupIndex, uint group_id : SV_GroupID)
{
    Buffer<uint> tileListBuffer = ResourceDescriptorHeap[c_tileListBuffer];
    uint packed_coords = tileListBuffer[group_id];
    int2 dispatch_thread_id = int2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    int2 dispatch_group_id = dispatch_thread_id / 8;
    uint2 remapped_group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(group_index);
    uint2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;
    
    FFX_DNSR_Reflections_Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, SceneCB.renderSize, c_temporalStability, 32);
}