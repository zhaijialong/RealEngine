#include "common.hlsli"

cbuffer CB : register(b1)
{
    uint c_inputRT;
    uint c_historyInputRT;
    uint c_velocityRT;
    uint c_linearDepthRT;
    uint c_prevLinearDepthRT;

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
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];

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
float3 VarianceClip(float3 history, float3 c0, float3 c1, float3 c2, float3 c3, float3 c4, float3 c5, float3 c6, float3 c7, float3 c8)
{
    float3 m1 = c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8;
    float3 m2 = c0 * c0 + c1 * c1 + c2 * c2 + c3 * c3 + c4 * c4 + c5 * c5 + c6 * c6 + c7 * c7 + c8 * c8;

    const float gamma = 1.0f;

    float3 mu = m1 / 9.0;
    float3 sigma = sqrt(abs(m2 / 9.0 - mu * mu));
    float3 min = mu - gamma * sigma;
    float3 max = mu + gamma * sigma;

    return clamp(history, min, max);
}

float3 GetVelocity(uint2 screenPos, float linearDepth)
{
    Texture2D linearDepthRT = ResourceDescriptorHeap[c_linearDepthRT];
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityRT];

#if 0
    float4 velocity = velocityTexture[screenPos];
#else
    int2 closestPosOffset = int2(0, 0);
    float closestDepth = linearDepth;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            float depth = linearDepthRT[screenPos + int2(x, y)].x;
            if (depth < closestDepth)
            {
                closestDepth = depth;
                closestPosOffset = int2(x, y);
            }
        }
    }

    float4 velocity = velocityTexture[screenPos + closestPosOffset];
#endif

    if (any(abs(velocity) > 0.00001))
    {
        return UnpackVelocity(velocity);
    }
    
    float3 ndcPos = float3(GetNdcPosition((float2)screenPos + 0.5), GetNdcDepth(linearDepth));
    
    float4 prevClipPos = mul(CameraCB.mtxClipToPrevClipNoJitter, float4(ndcPos, 1.0));
    float3 prevNdcPos = GetNdcPosition(prevClipPos);

    return ndcPos - prevNdcPos;
}

float4 GetHistory(float2 uv)
{
    Texture2D historyInputTexture = ResourceDescriptorHeap[c_historyInputRT];

    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    float historyWeight = historyInputTexture.SampleLevel(linearSampler, uv, 0).a;

    float4 rtMetrics = float4(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight, SceneCB.viewWidth, SceneCB.viewHeight);
    float3 historyColor = BicubicFilter(historyInputTexture, uv, rtMetrics);

    if (c_historyInputRT == c_inputRT)
    {
        historyColor = Tonemap(historyColor); //first frame
        historyWeight = 0.0;
    }

    return float4(historyColor, historyWeight);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 screenPos = dispatchThreadID.xy;

    Texture2D inputTexture = ResourceDescriptorHeap[c_inputRT];
    Texture2D linearDepthRT = ResourceDescriptorHeap[c_linearDepthRT];
    RWTexture2D<unorm float4> historyOutputTexture = ResourceDescriptorHeap[c_historyOutputRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputRT];

    float3 inputColor = Tonemap(inputTexture[screenPos].xyz);

    float linearDepth = linearDepthRT[screenPos].x;
    float3 velocity = GetVelocity(screenPos, linearDepth);
    float2 prevNdcPos = GetNdcPosition((float2)screenPos + 0.5) - velocity.xy;
    float2 prevUV = GetScreenUV(prevNdcPos);

    if (any(prevUV < 0.0) || any(prevUV > 1.0))
    {
        //out of screen
        historyOutputTexture[screenPos] = float4(inputColor, 0.5);
        outputTexture[screenPos] = float4(TonemapInvert(inputColor), 1.0);
        return;
    }
    
    const int2 screenPosMin = int2(0, 0);
    const int2 screenPosMax = int2(SceneCB.viewWidth, SceneCB.viewHeight) - 1;

    float3 c0 = Tonemap(inputTexture[clamp(screenPos + int2(-1, -1), screenPosMin, screenPosMax)].xyz);
    float3 c1 = Tonemap(inputTexture[clamp(screenPos + int2(0, -1), screenPosMin, screenPosMax)].xyz);
    float3 c2 = Tonemap(inputTexture[clamp(screenPos + int2(1, -1), screenPosMin, screenPosMax)].xyz);
    float3 c3 = Tonemap(inputTexture[clamp(screenPos + int2(-1, 0), screenPosMin, screenPosMax)].xyz);
    float3 c4 = inputColor;
    float3 c5 = Tonemap(inputTexture[clamp(screenPos + int2(1, 0), screenPosMin, screenPosMax)].xyz);
    float3 c6 = Tonemap(inputTexture[clamp(screenPos + int2(-1, 1), screenPosMin, screenPosMax)].xyz);
    float3 c7 = Tonemap(inputTexture[clamp(screenPos + int2(0, 1), screenPosMin, screenPosMax)].xyz);
    float3 c8 = Tonemap(inputTexture[clamp(screenPos + int2(1, 1), screenPosMin, screenPosMax)].xyz);

    float4 historyColor = GetHistory(prevUV);
    historyColor.xyz = VarianceClip(historyColor.xyz, c0, c1, c2, c3, c4, c5, c6, c7, c8);

    float luma = Luminance(inputColor);
    float prevLuma = Luminance(historyColor.xyz);
    float lumaDiff = saturate(abs(luma - prevLuma) / max(luma, prevLuma));

    float weight = historyColor.w * (1.0 - lumaDiff) * (1.0 - lumaDiff);
    float newWeight = saturate(1.0 / (2.0 - weight)); //[0.5, 1.0)

    float3 ouputColor = lerp(inputColor, historyColor.xyz, lerp(0.8, 0.99, newWeight));

    historyOutputTexture[screenPos] = float4(ouputColor, newWeight);
    outputTexture[screenPos] = float4(TonemapInvert(ouputColor), 1.0);
}
