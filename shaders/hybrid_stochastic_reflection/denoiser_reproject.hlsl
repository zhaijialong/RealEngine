#include "hsr_common.hlsli"

cbuffer Constants : register(b0)
{
    uint c_tileListBuffer;
    uint c_inputTexture;
    uint c_ouputTexture;
};

float FFX_DNSR_Reflections_LoadDepth(int2 pixel_coordinate)
{
    return 0.0;
}

float FFX_DNSR_Reflections_LoadDepthHistory(int2 pixel_coordinate)
{
    return 0.0;
}

float FFX_DNSR_Reflections_SampleDepthHistory(float2 uv)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_LoadRadiance(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_LoadRadianceHistory(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_SampleRadianceHistory(float2 uv)
{
    return 0.0;
}

float16_t FFX_DNSR_Reflections_SampleNumSamplesHistory(float2 uv)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_LoadWorldSpaceNormal(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t3 FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(float2 uv)
{
    return 0.0;
}

float16_t FFX_DNSR_Reflections_LoadRoughness(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t FFX_DNSR_Reflections_SampleRoughnessHistory(float2 uv)
{
    return 0.0;
}

float2 FFX_DNSR_Reflections_LoadMotionVector(int2 pixel_coordinate)
{
    return 0.0;
}

float16_t FFX_DNSR_Reflections_SampleVarianceHistory(float2 uv)
{
    return 0.0;
}

float16_t FFX_DNSR_Reflections_LoadRayLength(int2 pixel_coordinate)
{
    return 0.0;
}

void FFX_DNSR_Reflections_StoreRadianceReprojected(int2 pixel_coordinate, float16_t3 value)
{
}

void FFX_DNSR_Reflections_StoreAverageRadiance(int2 pixel_coordinate, float16_t3 value)
{
}

void FFX_DNSR_Reflections_StoreVariance(int2 pixel_coordinate, float16_t value)
{
}

void FFX_DNSR_Reflections_StoreNumSamples(int2 pixel_coordinate, float16_t value)
{
}

#include "ffx-reflection-dnsr/ffx_denoiser_reflections_reproject.h"

[numthreads(8, 8, 1)]
void main(int2 group_thread_id : SV_GroupThreadID,
                uint group_index : SV_GroupIndex,
                uint group_id : SV_GroupID)
{
    Buffer<uint> tileListBuffer = ResourceDescriptorHeap[c_tileListBuffer];
    uint packed_coords = tileListBuffer[group_id];
    int2 dispatch_thread_id = int2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    int2 dispatch_group_id = dispatch_thread_id / 8;
    uint2 remapped_group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(group_index);
    uint2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;
    
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2D<float4> ouputTexture = ResourceDescriptorHeap[c_ouputTexture];

    ouputTexture[remapped_dispatch_thread_id] = inputTexture[remapped_dispatch_thread_id];
}