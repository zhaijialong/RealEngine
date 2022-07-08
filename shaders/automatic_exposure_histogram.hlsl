#include "common.hlsli"
#include "exposure.hlsli"

// reference : https://alextardif.com/HistogramLuminance.html

#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16
#define HISTOGRAM_BIN_NUM (GROUP_SIZE_X * GROUP_SIZE_Y)

uint GetHistogramBin(float luminance, float minLuminance, float maxLuminance)
{
    float position = saturate((luminance - minLuminance) / (maxLuminance - minLuminance));
    return uint(pow(position, 1.0 / 5.0) * (HISTOGRAM_BIN_NUM - 1)); //todo : maybe log2(luminance) will have better distribution among the histogram bins
}

float GetLuminance(uint histogramBin, float minLuminance, float maxLuminance)
{
    float position = ((float)histogramBin + 0.5) / float(HISTOGRAM_BIN_NUM - 1);
    return pow(position, 5.0) * (maxLuminance - minLuminance);
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
        SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

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
    float lowPercentile;
    float highPercentile;
};
ConstantBuffer<ReduceHistogramConstants> ReductionCB : register(b1);

groupshared float s_values[HISTOGRAM_BIN_NUM];
groupshared float s_intermediateValues[HISTOGRAM_BIN_NUM]; //used for sum reduction

float ComputeAverageLuminance(float lowPercentileSum, float highPercentileSum)
{
    float sumLuminance = 0.0f;
    float sumWeight = 0.0f;

    [unroll]
    for (uint i = 0; i < HISTOGRAM_BIN_NUM; ++i)
    {
        float histogramValue = s_values[i];
        float luminance = GetLuminance(i, ReductionCB.minLuminance, ReductionCB.maxLuminance);

        // remove dark values
        float off = min(lowPercentileSum, histogramValue);
        lowPercentileSum -= off;
        highPercentileSum -= off;
        histogramValue -= off;

        // remove highlight values
        histogramValue = min(highPercentileSum, histogramValue);
        highPercentileSum -= histogramValue;

        sumLuminance += histogramValue * luminance;
        sumWeight += histogramValue;
    }

    return sumLuminance / max(sumWeight, 0.0001);
}

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void histogram_reduction(uint groupIndex : SV_GroupIndex)
{
    ByteAddressBuffer histogramBuffer = ResourceDescriptorHeap[ReductionCB.histogramBufferSRV];
    float histogramValue = histogramBuffer.Load(groupIndex * 4) / 1024.0f;

    s_values[groupIndex] = histogramValue;
    s_intermediateValues[groupIndex] = histogramValue;
    
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint i = (HISTOGRAM_BIN_NUM >> 1); i > 0; i >>= 1)
    {
        if (groupIndex < i)
        {
            s_intermediateValues[groupIndex] += s_intermediateValues[groupIndex + i];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    float histogramValueSum = s_intermediateValues[0];

    if (groupIndex == 0)
    {
        RWTexture2D<float> avgLuminanceTexture = ResourceDescriptorHeap[ReductionCB.avgLuminanceTextureUAV];
        avgLuminanceTexture[uint2(0, 0)] = ComputeAverageLuminance(histogramValueSum * ReductionCB.lowPercentile, histogramValueSum * ReductionCB.highPercentile);
    }
}