#include "hsr_common.hlsli"

cbuffer Constants : register(b1)
{
    uint c_tileListBuffer;
    uint c_normalTexture;
    uint c_inputRadianceTexture;
    uint c_reprojectedRadianceTexture;
    uint c_avgRadianceTexture;
    uint c_inputVarianceTexture;
    uint c_sampleCountTexture;
    uint c_outputRadianceUAV;
    uint c_outputVarianceUAV;
};

float16_t3 FFX_DNSR_Reflections_SampleAverageRadiance(float2 uv) 
{
    Texture2D avgRadianceTexture = ResourceDescriptorHeap[c_avgRadianceTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    return (float16_t3)avgRadianceTexture.SampleLevel(linearSampler, uv, 0.0f).xyz;
}

float16_t3 FFX_DNSR_Reflections_LoadRadiance(int2 pixel_coordinate) 
{ 
    Texture2D inputRadianceTexture = ResourceDescriptorHeap[c_inputRadianceTexture];
    return (float16_t3)inputRadianceTexture[pixel_coordinate].xyz;
}

float16_t3 FFX_DNSR_Reflections_LoadRadianceReprojected(int2 pixel_coordinate) 
{ 
    Texture2D reprojectedRadianceTexture = ResourceDescriptorHeap[c_reprojectedRadianceTexture];
    return (float16_t3)reprojectedRadianceTexture[pixel_coordinate].xyz;
}

float16_t FFX_DNSR_Reflections_LoadRoughness(int2 pixel_coordinate) 
{ 
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return (float16_t)normalTexture[pixel_coordinate].w;
}

float16_t FFX_DNSR_Reflections_LoadVariance(int2 pixel_coordinate) 
{ 
    Texture2D<float> inputVarianceTexture = ResourceDescriptorHeap[c_inputVarianceTexture];
    float16_t variance = (float16_t)inputVarianceTexture[pixel_coordinate].x;
    
    if (isnan(variance))
    {
        variance = 1.0; //to fix artifacts after resolution changed
    }
    
    return variance;
}

float16_t FFX_DNSR_Reflections_LoadNumSamples(int2 pixel_coordinate) 
{
    Texture2D<float> sampleCountTexture = ResourceDescriptorHeap[c_sampleCountTexture];
    return (float16_t)sampleCountTexture[pixel_coordinate].x;
}

void FFX_DNSR_Reflections_StoreTemporalAccumulation(int2 pixel_coordinate, float16_t3 radiance, float16_t variance) 
{
    RWTexture2D<float3> outputRadianceUAV = ResourceDescriptorHeap[c_outputRadianceUAV];
    RWTexture2D<float> outputVarianceUAV = ResourceDescriptorHeap[c_outputVarianceUAV];
    outputRadianceUAV[pixel_coordinate] = radiance;
    outputVarianceUAV[pixel_coordinate] = variance;
}

#include "ffx-reflection-dnsr/ffx_denoiser_reflections_resolve_temporal.h"

[numthreads(8, 8, 1)]
void main(int2 group_thread_id : SV_GroupThreadID, uint group_index : SV_GroupIndex, uint group_id : SV_GroupID) 
{
    Buffer<uint> tileListBuffer = ResourceDescriptorHeap[c_tileListBuffer];
    uint packed_coords = tileListBuffer[group_id];
    int2 dispatch_thread_id = int2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    int2 dispatch_group_id = dispatch_thread_id / 8;
    uint2 remapped_group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(group_index);
    uint2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    FFX_DNSR_Reflections_ResolveTemporal(remapped_dispatch_thread_id, remapped_group_thread_id, 
        SceneCB.renderSize, SceneCB.rcpRenderSize, c_temporalStability);
}