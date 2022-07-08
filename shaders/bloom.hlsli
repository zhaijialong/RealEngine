#pragma once

#include "common.hlsli"

// reference : Next Generation Post Processing in Call of Duty: Advanced Warfare [Jimenez14]
//             http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare

float3 Sample(Texture2D texture, float2 uv, float threshold)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    float3 color = texture.SampleLevel(linearSampler, uv, 0).xyz;

#if FIRST_PASS
    if(any(isnan(color)))
    {
        return 0.0;
    }

    float luminance = Luminance(color);
    float a = saturate(luminance - threshold);
    color *= a * a;
#endif

    return color;
}

float3 PartialAverage(float3 a, float3 b, float3 c, float3 d)
{
#if FIRST_PASS
    // Karis average
    float w0 = rcp(1.0 + Luminance(a));
    float w1 = rcp(1.0 + Luminance(b));
    float w2 = rcp(1.0 + Luminance(c));
    float w3 = rcp(1.0 + Luminance(d));
#else
    float w0 = 1.0;
    float w1 = 1.0;
    float w2 = 1.0;
    float w3 = 1.0;
#endif

    return (a * w0 + b * w1 + c * w2 + d * w3) / (w0 + w1 + w2 + w3);
}

float3 BloomDownsample(Texture2D texture, float2 uv, float2 pixelSize, float threshold)
{
    // CoD's 13-tap convolution
    float3 c0 = Sample(texture, uv + float2(-2.0, 2.0) * pixelSize, threshold);
    float3 c1 = Sample(texture, uv + float2(0.0, 2.0) * pixelSize, threshold);
    float3 c2 = Sample(texture, uv + float2(2.0, 2.0) * pixelSize, threshold);
    float3 c3 = Sample(texture, uv + float2(-1.0, 1.0) * pixelSize, threshold);
    float3 c4 = Sample(texture, uv + float2(1.0, 1.0) * pixelSize, threshold);
    float3 c5 = Sample(texture, uv + float2(-2.0, 0.0) * pixelSize, threshold);
    float3 c6 = Sample(texture, uv + float2(0.0, 0.0) * pixelSize, threshold);
    float3 c7 = Sample(texture, uv + float2(2.0, 0.0) * pixelSize, threshold);
    float3 c8 = Sample(texture, uv + float2(-1.0, -1.0) * pixelSize, threshold);
    float3 c9 = Sample(texture, uv + float2(1.0, -1.0) * pixelSize, threshold);
    float3 c10 = Sample(texture, uv + float2(-2.0, -2.0) * pixelSize, threshold);
    float3 c11 = Sample(texture, uv + float2(0.0, -2.0) * pixelSize, threshold);
    float3 c12 = Sample(texture, uv + float2(2.0, -2.0) * pixelSize, threshold);

    float3 output = PartialAverage(c3, c4, c8, c9) * 0.5;
    output += PartialAverage(c0, c1, c5, c6) * 0.125;
    output += PartialAverage(c1, c2, c6, c7) * 0.125;
    output += PartialAverage(c5, c6, c10, c11) * 0.125;
    output += PartialAverage(c6, c7, c11, c12) * 0.125;

    return output;
}

float3 BloomUpsample(Texture2D texture, float2 uv, float2 pixelSize)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearBlackBoarderSampler];

    // 3x3 tent filter
    float3 c0 = texture.SampleLevel(linearSampler, uv + float2(-1.0, 1.0) * pixelSize, 0).xyz * 1.0;
    float3 c1 = texture.SampleLevel(linearSampler, uv + float2(0.0, 1.0) * pixelSize, 0).xyz * 2.0;
    float3 c2 = texture.SampleLevel(linearSampler, uv + float2(1.0, 1.0) * pixelSize, 0).xyz * 1.0;
    float3 c3 = texture.SampleLevel(linearSampler, uv + float2(-1.0, 0.0) * pixelSize, 0).xyz * 2.0;
    float3 c4 = texture.SampleLevel(linearSampler, uv + float2(0.0, 0.0) * pixelSize, 0).xyz * 4.0;
    float3 c5 = texture.SampleLevel(linearSampler, uv + float2(1.0, 0.0) * pixelSize, 0).xyz * 2.0;
    float3 c6 = texture.SampleLevel(linearSampler, uv + float2(-1.0, -1.0) * pixelSize, 0).xyz * 1.0;
    float3 c7 = texture.SampleLevel(linearSampler, uv + float2(0.0, -1.0) * pixelSize, 0).xyz * 2.0;
    float3 c8 = texture.SampleLevel(linearSampler, uv + float2(1.0, -1.0) * pixelSize, 0).xyz * 1.0;

    return (c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8) / 16.0;
}