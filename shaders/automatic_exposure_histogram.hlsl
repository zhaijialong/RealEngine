#include "common.hlsli"
#include "exposure.hlsli"

// reference : https://alextardif.com/HistogramLuminance.html

#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 8
#define HISTOGRAM_BIN_NUM (GROUP_SIZE_X * GROUP_SIZE_Y)

uint GetHistogramBin(float luminance, float minLuminance, float maxLuminance)
{
    return uint(saturate((luminance - minLuminance) / (maxLuminance - minLuminance)) * (HISTOGRAM_BIN_NUM - 1));
}

float GetLuminance(uint histogramBin, float minLuminance, float maxLuminance)
{
    return ((float)histogramBin + 0.5) / float(HISTOGRAM_BIN_NUM - 1) * (maxLuminance - minLuminance);
}


struct BuildHistogramConstants
{
    uint inputTextureSRV;
    uint histogramBufferUAV;
    uint width;
    uint height;
    float rcpWidth;
    float rcpHeight;
    float minLuminance;
    float maxLuminance;
};
ConstantBuffer<BuildHistogramConstants> BuildCB : register(b1);

groupshared uint s_HistogramBins[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void build_histogram(uint groupIndex : SV_GroupIndex, uint3 dispatchThreadID : SV_DispatchThreadID)
{
    s_HistogramBins[groupIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (all(dispatchThreadID.xy < uint2(BuildCB.width, BuildCB.height)))
    {
        Texture2D inputTexture = ResourceDescriptorHeap[BuildCB.inputTextureSRV];
        SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];

        float2 screenPos = (float2) dispatchThreadID.xy + 0.5;
        float2 uv = screenPos * float2(BuildCB.rcpWidth, BuildCB.rcpHeight);
        float3 color = inputTexture.SampleLevel(linearSampler, uv, 0).xyz;

        float luminance = clamp(Luminance(color), BuildCB.minLuminance, BuildCB.maxLuminance);
        uint bin = GetHistogramBin(luminance, BuildCB.minLuminance, BuildCB.maxLuminance);
        float weight = GetMeteringWeight(screenPos, float2(BuildCB.width, BuildCB.height));
        
        InterlockedAdd(s_HistogramBins[bin], uint(weight * 1024.0));
    }
    GroupMemoryBarrierWithGroupSync();

    RWByteAddressBuffer histogramBuffer = ResourceDescriptorHeap[BuildCB.histogramBufferUAV];
    histogramBuffer.InterlockedAdd(groupIndex * 4, s_HistogramBins[groupIndex]);
}

struct ReduceHistogramConstants
{
    uint histogramBufferSRV;
    uint avgLuminanceTextureUAV;
    float minLuminance;
    float maxLuminance;
};
ConstantBuffer<ReduceHistogramConstants> ReductionCB : register(b0);

groupshared float s_Luminance[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void histogram_reduction(uint groupIndex : SV_GroupIndex)
{
    ByteAddressBuffer histogramBuffer = ResourceDescriptorHeap[ReductionCB.histogramBufferSRV];
    uint histogramCount = histogramBuffer.Load(groupIndex * 4);
    float luminance = GetLuminance(groupIndex, ReductionCB.minLuminance, ReductionCB.maxLuminance);

    s_HistogramBins[groupIndex] = histogramCount;
    s_Luminance[groupIndex] = histogramCount * luminance;

    GroupMemoryBarrierWithGroupSync();

    // sum reduction
    [unroll]
    for (uint histogramSampleIndex = (HISTOGRAM_BIN_NUM >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1)
    {
        if (groupIndex < histogramSampleIndex)
        {
            s_HistogramBins[groupIndex] += s_HistogramBins[groupIndex + histogramSampleIndex];
            s_Luminance[groupIndex] += s_Luminance[groupIndex + histogramSampleIndex];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if(groupIndex == 0)
    {
        RWTexture2D<float> avgLuminanceTexture = ResourceDescriptorHeap[ReductionCB.avgLuminanceTextureUAV];
        avgLuminanceTexture[uint2(0, 0)] = s_Luminance[0] / s_HistogramBins[0];
    }
}