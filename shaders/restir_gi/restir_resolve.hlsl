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
    uint c_outputVarianceTexture;
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
#if ENABLE_RESTIR
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
#endif
    Texture2D<uint2> halfDepthNormal = ResourceDescriptorHeap[c_halfDepthNormal];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
#if OUTPUT_SH
    RWTexture2D<uint4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    RWTexture2D<float> outputVarianceTexture = ResourceDescriptorHeap[c_outputVarianceTexture];
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
    
#if ENABLE_RESTIR
    float4 M = reservoirTexture.GatherRed(pointSampler, gatherUV);
    float4 W = reservoirTexture.GatherGreen(pointSampler, gatherUV);
#else
    float4 M = 1.0;
    float4 W = 1.0;
#endif
    
    uint4 sampleDepth = halfDepthNormal.GatherRed(pointSampler, gatherUV);
    uint4 sampleNormal = halfDepthNormal.GatherGreen(pointSampler, gatherUV);
    
    float3 radiance00 = radianceTexture[uint2(bilinearFilter.origin) + uint2(0, 0)].xyz * W.w;
    float3 radiance10 = radianceTexture[uint2(bilinearFilter.origin) + uint2(1, 0)].xyz * W.z;
    float3 radiance01 = radianceTexture[uint2(bilinearFilter.origin) + uint2(0, 1)].xyz * W.x;
    float3 radiance11 = radianceTexture[uint2(bilinearFilter.origin) + uint2(1, 1)].xyz * W.y;
    
    float4 customWeights;
    customWeights.x = ComputeCustomWeight(depth, N, asfloat(sampleDepth.w), DecodeNormal16x2(sampleNormal.w));
    customWeights.y = ComputeCustomWeight(depth, N, asfloat(sampleDepth.z), DecodeNormal16x2(sampleNormal.z));
    customWeights.z = ComputeCustomWeight(depth, N, asfloat(sampleDepth.x), DecodeNormal16x2(sampleNormal.x));
    customWeights.w = ComputeCustomWeight(depth, N, asfloat(sampleDepth.y), DecodeNormal16x2(sampleNormal.y));
    
    float4 bilinearWeights = GetBilinearCustomWeights(bilinearFilter, customWeights);
    
#if OUTPUT_SH
    uint4 rayDirection = rayDirectionTexture.GatherRed(pointSampler, gatherUV);
    float3 rayDirection00 = DecodeNormal16x2(rayDirection.w);
    float3 rayDirection10 = DecodeNormal16x2(rayDirection.z);
    float3 rayDirection01 = DecodeNormal16x2(rayDirection.x);
    float3 rayDirection11 = DecodeNormal16x2(rayDirection.y);
    
    SH sh00, sh10, sh01, sh11;
    sh00.Project(radiance00, rayDirection00);
    sh10.Project(radiance10, rayDirection10);
    sh01.Project(radiance01, rayDirection01);
    sh11.Project(radiance11, rayDirection11);
    
    SH sh = ApplyBilinearCustomWeights(sh00, sh10, sh01, sh11, bilinearWeights);
    outputTexture[pos] = PackSH(sh);

    float sampleCount = ApplyBilinearCustomWeights(M.w, M.z, M.x, M.y, bilinearWeights);
    outputVarianceTexture[pos] = square(1.0 - saturate(sampleCount / 500.0));
#else
    float3 radiance = ApplyBilinearCustomWeights(radiance00, radiance10, radiance01, radiance11, bilinearWeights);
    outputTexture[pos] = float4(radiance, 0.0);
#endif
}