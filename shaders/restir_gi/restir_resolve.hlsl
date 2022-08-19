#include "../common.hlsli"
#include "../bilinear.hlsli"
#include "../recurrent_blur/sh.hlsli"

cbuffer CB : register(b1)
{
    uint c_reservoirTexture;
    uint c_radianceTexture;
    uint c_rayDirectionTexture;
    uint c_halfDepthNormal;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_outputTexture;
}

float ComputeCustomWeight(float depth, float3 normal, float sampleDepth, float3 sampleNormal)
{
    float linearDepth = GetLinearDepth(depth);
    float linearSampleDepth = GetLinearDepth(sampleDepth);
    float depthWeight = exp(-abs(linearDepth - linearSampleDepth));
    
    float normalWeight = saturate(dot(normal, sampleNormal));

    return depthWeight * normalWeight;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D radianceTexture = ResourceDescriptorHeap[c_radianceTexture];
    Texture2D<uint> rayDirectionTexture = ResourceDescriptorHeap[c_rayDirectionTexture];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
    Texture2D<uint2> halfDepthNormal = ResourceDescriptorHeap[c_halfDepthNormal];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
#if OUTPUT_SH
    RWTexture2D<uint4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
#else
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
#endif
    
    uint2 pos = dispatchThreadID.xy;
    float2 uv = GetScreenUV(pos, SceneCB.rcpRenderSize);
    
    float depth = depthTexture[pos];
    float3 N = DecodeNormal(normalTexture[pos].xyz);
    
    if(depth == 0.0)
    {
        outputTexture[pos] = 0;
        return;
    }
    
    uint2 halfRenderSize = (SceneCB.renderSize + 1) / 2;
    Bilinear bilinearFilter = GetBilinearFilter(uv, halfRenderSize);
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];

    float2 gatherUV = (bilinearFilter.origin + 1.0) / halfRenderSize;
    
    float4 radianceRed = radianceTexture.GatherRed(pointSampler, gatherUV);
    float4 radianceGreen = radianceTexture.GatherGreen(pointSampler, gatherUV);
    float4 radianceBlue = radianceTexture.GatherBlue(pointSampler, gatherUV);
    float4 W = reservoirTexture.GatherGreen(pointSampler, gatherUV);
    
    uint4 sampleDepth = halfDepthNormal.GatherRed(pointSampler, gatherUV);
    uint4 sampleNormal = halfDepthNormal.GatherGreen(pointSampler, gatherUV);
    
    float3 radiance00 = float3(radianceRed.x, radianceGreen.x, radianceBlue.x) * W.x;
    float3 radiance10 = float3(radianceRed.y, radianceGreen.y, radianceBlue.y) * W.y;
    float3 radiance01 = float3(radianceRed.z, radianceGreen.z, radianceBlue.z) * W.z;
    float3 radiance11 = float3(radianceRed.w, radianceGreen.w, radianceBlue.w) * W.w;
    
    float4 customWeights;
    customWeights.x = ComputeCustomWeight(depth, N, asfloat(sampleDepth.x), DecodeNormal16x2(sampleNormal.x));
    customWeights.y = ComputeCustomWeight(depth, N, asfloat(sampleDepth.y), DecodeNormal16x2(sampleNormal.y));
    customWeights.z = ComputeCustomWeight(depth, N, asfloat(sampleDepth.z), DecodeNormal16x2(sampleNormal.z));
    customWeights.w = ComputeCustomWeight(depth, N, asfloat(sampleDepth.w), DecodeNormal16x2(sampleNormal.w));
    
    float4 bilinearWeights = GetBilinearCustomWeights(bilinearFilter, customWeights);
    
#if OUTPUT_SH
    uint4 rayDirection = rayDirectionTexture.GatherGreen(pointSampler, gatherUV);
    float3 rayDirection00 = DecodeNormal16x2(rayDirection.x);
    float3 rayDirection10 = DecodeNormal16x2(rayDirection.y);
    float3 rayDirection01 = DecodeNormal16x2(rayDirection.z);
    float3 rayDirection11 = DecodeNormal16x2(rayDirection.w);
    
    SH sh00, sh10, sh01, sh11;
    sh00.Evaluate(radiance00, rayDirection00);
    sh10.Evaluate(radiance10, rayDirection10);
    sh01.Evaluate(radiance01, rayDirection01);
    sh11.Evaluate(radiance11, rayDirection11);
    
    SH sh = ApplyBilinearCustomWeights(sh00, sh10, sh01, sh11, bilinearWeights);
    outputTexture[pos] = PackSH(sh);
#else
    float3 radiance = ApplyBilinearCustomWeights(radiance00, radiance10, radiance01, radiance11, bilinearWeights);
    outputTexture[pos] = float4(radiance, 0.0);
#endif
}