#include "common.hlsli"

cbuffer CB : register(b1)
{
    uint c_inputRT;
    uint c_historyInputRT;
    uint c_velocityRT;
    uint c_linearDepthRT;

    uint c_historyOutputRT;
    uint c_outputRT;
};

// https://gpuopen.com/learn/optimized-reversible-tonemapper-for-resolve/
float max3(float x, float y, float z) { return max(x, max(y, z)); }
float3 Tonemap(float3 c) { return c * rcp(max3(c.r, c.g, c.b) + 1.0); }
float3 TonemapInvert(float3 c) { return c * rcp(1.0 - max3(c.r, c.g, c.b)); }

// Filmic SMAA Sharp Morphological and Temporal Antialiasing. Jorge Jimenez[2017]
float3 BicubicFilter(Texture2D colorTex, float2 texcoord, float4 rtMetrics)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

    float2 position = rtMetrics.zw * texcoord;
    float2 centerPosition = floor(position - 0.5) + 0.5;
    float2 f = position - centerPosition;
    float2 f2 = f * f;
    float2 f3 = f * f2;

    float sharpeness = 50.0;
    float c = sharpeness / 100.0;
    float2 w0 = -c * f3 + 2.0 * c * f2 - c * f;
    float2 w1 = (2.0 - c) * f3 - (3.0 - c) * f2 + 1.0;
    float2 w2 = -(2.0 - c) * f3 + (3.0 - 2.0 * c) * f2 + c * f;
    float2 w3 = c * f3 - c * f2;

    float2 w12 = w1 + w2;
    float2 tc12 = rtMetrics.xy * (centerPosition + w2 / w12);
    float3 centerColor = colorTex.SampleLevel(linearSampler, float2(tc12.x, tc12.y), 0.0).rgb;

    float2 tc0 = rtMetrics.xy * (centerPosition - 1.0);
    float2 tc3 = rtMetrics.xy * (centerPosition + 2.0);
    float4 color = float4(colorTex.SampleLevel(linearSampler, float2(tc12.x, tc0.y), 0.0).rgb, 1.0) * (w12.x * w0.y) +
                   float4(colorTex.SampleLevel(linearSampler, float2(tc0.x, tc12.y), 0.0).rgb, 1.0) * (w0.x * w12.y) +
                   float4(centerColor, 1.0) * (w12.x * w12.y) +
                   float4(colorTex.SampleLevel(linearSampler, float2(tc3.x, tc12.y), 0.0).rgb, 1.0) * (w3.x * w12.y) +
                   float4(colorTex.SampleLevel(linearSampler, float2(tc12.x, tc3.y), 0.0).rgb, 1.0) * (w12.x * w3.y);
    return color.rgb * rcp(color.a);
}

// An Excursion in Temporal Supersampling. Marco Salvi[2016]
float3 VarianceClip(float3 history, float gamma, float3 c0, float3 c1, float3 c2, float3 c3, float3 c4, float3 c5, float3 c6, float3 c7, float3 c8)
{
    float3 m1 = c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8;
    float3 m2 = c0 * c0 + c1 * c1 + c2 * c2 + c3 * c3 + c4 * c4 + c5 * c5 + c6 * c6 + c7 * c7 + c8 * c8;

    float3 mu = m1 / 9.0;
    float3 sigma = sqrt(abs(m2 / 9.0 - mu * mu));
    float3 min = mu - gamma * sigma;
    float3 max = mu + gamma * sigma;

    return clamp(history, min, max);
}

float2 GetVelocity(uint2 screenPos)
{
    Texture2D linearDepthRT = ResourceDescriptorHeap[c_linearDepthRT];
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityRT];

    int2 closestPosOffset = int2(0, 0);
    float closestDepth = 1e8;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float depth = linearDepthRT[screenPos + int2(x, y)].x;
            if (depth < closestDepth)
            {
                closestDepth = depth;
                closestPosOffset = int2(x, y);
            }
        }
    }

    int2 closestScreenPos = screenPos + closestPosOffset;
    float2 velocity = velocityTexture[closestScreenPos].xy;

    return velocity;
}

float4 GetHistory(float2 uv)
{
    Texture2D historyInputTexture = ResourceDescriptorHeap[c_historyInputRT];

    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    float historyWeight = historyInputTexture.SampleLevel(linearSampler, uv, 0).a;

    float4 rtMetrics = float4(SceneCB.rcpDisplaySize, SceneCB.displaySize);
    float3 historyColor = BicubicFilter(historyInputTexture, uv, rtMetrics);

    return float4(saturate(historyColor), historyWeight);
}

float GetVelocityConfidenceFactor(float2 velocity)
{
    // Difference in pixels for velocity after which the pixel is marked as no-history
    const uint FRAME_VELOCITY_IN_PIXELS_DIFF = 128;

    //velocity.xy is in ndc space
    float2 velocityInPixels = velocity * 0.5 * float2(SceneCB.displaySize);

    return saturate(1.0 - length(velocityInPixels) / FRAME_VELOCITY_IN_PIXELS_DIFF);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 screenPos = dispatchThreadID.xy;

    Texture2D inputTexture = ResourceDescriptorHeap[c_inputRT];
    RWTexture2D<unorm float4> historyOutputTexture = ResourceDescriptorHeap[c_historyOutputRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputRT];

    float3 inputColor = Tonemap(inputTexture[screenPos].xyz);

    float2 velocity = GetVelocity(screenPos);
    float2 prevUV = GetScreenUV(screenPos, SceneCB.rcpDisplaySize) - velocity * float2(0.5, -0.5); //velocity.xy is in ndc space

    bool firstFrame = c_historyInputRT == c_inputRT;
    bool outOfBound = any(prevUV < 0.0) || any(prevUV > 1.0);
    float velocityConfidenceFactor = GetVelocityConfidenceFactor(velocity);
    bool historyValid = !firstFrame && !outOfBound && (velocityConfidenceFactor > 0.0);
    
    const int2 screenPosMin = int2(0, 0);
    const int2 screenPosMax = int2(SceneCB.displaySize) - 1;
    float3 c0 = Tonemap(inputTexture[clamp(screenPos + int2(-1, -1), screenPosMin, screenPosMax)].xyz);
    float3 c1 = Tonemap(inputTexture[clamp(screenPos + int2(0, -1), screenPosMin, screenPosMax)].xyz);
    float3 c2 = Tonemap(inputTexture[clamp(screenPos + int2(1, -1), screenPosMin, screenPosMax)].xyz);
    float3 c3 = Tonemap(inputTexture[clamp(screenPos + int2(-1, 0), screenPosMin, screenPosMax)].xyz);
    float3 c4 = inputColor;
    float3 c5 = Tonemap(inputTexture[clamp(screenPos + int2(1, 0), screenPosMin, screenPosMax)].xyz);
    float3 c6 = Tonemap(inputTexture[clamp(screenPos + int2(-1, 1), screenPosMin, screenPosMax)].xyz);
    float3 c7 = Tonemap(inputTexture[clamp(screenPos + int2(0, 1), screenPosMin, screenPosMax)].xyz);
    float3 c8 = Tonemap(inputTexture[clamp(screenPos + int2(1, 1), screenPosMin, screenPosMax)].xyz);

    if (historyValid)
    {
        float4 historyColor = GetHistory(prevUV);

        const float varianceGamma = lerp(0.75, 2, velocityConfidenceFactor * velocityConfidenceFactor);
        historyColor.xyz = VarianceClip(historyColor.xyz, varianceGamma, c0, c1, c2, c3, c4, c5, c6, c7, c8);

        float weight = historyColor.w * velocityConfidenceFactor;

        float3 ouputColor = lerp(inputColor, historyColor.xyz, weight);
        float newWeight = saturate(1.0 / (2.0 - weight));

        historyOutputTexture[screenPos] = float4(ouputColor, newWeight);
        outputTexture[screenPos] = float4(TonemapInvert(ouputColor), 1.0);
    }
    else
    {
        float3 ouputColor = c0 * 0.0625 + c1 * 0.125 + c2 * 0.0625 + c3 * 0.125 + c4 * 0.25 + c5 * 0.125 + c6 * 0.0625 + c7 * 0.125 + c8 * 0.0625;

        historyOutputTexture[screenPos] = float4(ouputColor, 0.5);
        outputTexture[screenPos] = float4(TonemapInvert(ouputColor), 1.0);
    }
}
