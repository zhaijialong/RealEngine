#include "../common.hlsli"
#include "../bilinear.hlsli"

cbuffer CB : register(b1)
{
    uint c_reservoirTexture;
    uint c_radianceTexture;
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
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
    Texture2D<uint2> halfDepthNormal = ResourceDescriptorHeap[c_halfDepthNormal];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    uint2 pos = dispatchThreadID.xy;
    float2 uv = GetScreenUV(pos, SceneCB.rcpRenderSize);
    
    float depth = depthTexture[pos];
    float3 N = DecodeNormal(normalTexture[pos].xyz);
    
    if(depth == 0.0)
    {
        outputTexture[pos] = 0.xxxx;
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
    float3 radiance = ApplyBilinearCustomWeights(radiance00, radiance10, radiance01, radiance11, bilinearWeights);
    
    outputTexture[pos] = float4(radiance, 0.0);
}