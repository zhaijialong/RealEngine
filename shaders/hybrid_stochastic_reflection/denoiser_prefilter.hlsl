#include "hsr_common.hlsli"

cbuffer Constants : register(b1)
{
    uint c_tileListBuffer;
    uint c_linearDepthTexture;
    uint c_normalTexture;
    uint c_radianceTexture;
    uint c_varianceTexture;
    uint c_avgRadianceTexture;
    uint c_outputRadianceUAV;
    uint c_outputVarianceUAV;
};

float16_t3 FFX_DNSR_Reflections_SampleAverageRadiance(float2 uv) 
{
    Texture2D avgRadianceTexture = ResourceDescriptorHeap[c_avgRadianceTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    return (float16_t3)avgRadianceTexture.SampleLevel(linearSampler, uv, 0.0f).xyz;
}

float16_t FFX_DNSR_Reflections_LoadRoughness(int2 pixel_coordinate) 
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return (float16_t)normalTexture[pixel_coordinate].w;
}

void FFX_DNSR_Reflections_LoadNeighborhood(
    int2 pixel_coordinate,
    out float16_t3 radiance,
    out float16_t variance,
    out float16_t3 normal,
    out float depth,
    int2 screen_size) 
{
    Texture2D radianceTexture = ResourceDescriptorHeap[c_radianceTexture];
    Texture2D<float> varianceTexture = ResourceDescriptorHeap[c_varianceTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    Texture2D<float> linearDepthTexture = ResourceDescriptorHeap[c_linearDepthTexture];

    radiance = (float16_t3)radianceTexture[pixel_coordinate].xyz;
    variance = (float16_t)varianceTexture[pixel_coordinate].x;
    normal = (float16_t3)OctNormalDecode(normalTexture[pixel_coordinate].xyz);
    depth = (float16_t)linearDepthTexture[pixel_coordinate].x;
}

void FFX_DNSR_Reflections_StorePrefilteredReflections(int2 pixel_coordinate, float16_t3 radiance, float16_t variance) 
{
    RWTexture2D<float3> outputRadianceUAV = ResourceDescriptorHeap[c_outputRadianceUAV];
    RWTexture2D<float> outputVarianceUAV = ResourceDescriptorHeap[c_outputVarianceUAV];
    outputRadianceUAV[pixel_coordinate] = radiance;
    outputVarianceUAV[pixel_coordinate] = variance;
}

#include "ffx-reflection-dnsr/ffx_denoiser_reflections_prefilter.h"

[numthreads(8, 8, 1)]
void main(int2 group_thread_id : SV_GroupThreadID, uint group_index : SV_GroupIndex, uint group_id : SV_GroupID) 
{
    Buffer<uint> tileListBuffer = ResourceDescriptorHeap[c_tileListBuffer];
    uint packed_coords = tileListBuffer[group_id];
    int2 dispatch_thread_id = int2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    int2 dispatch_group_id = dispatch_thread_id / 8;
    uint2 remapped_group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(group_index);
    uint2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    FFX_DNSR_Reflections_Prefilter(remapped_dispatch_thread_id, remapped_group_thread_id, SceneCB.renderSize);
}