#include "../common.hlsli"
#include "../bilinear.hlsli"
#include "sh.hlsli"

cbuffer CB : register(b1)
{
    uint c_inputSHTexture;
    uint c_historySHTexture;
    uint c_historyAccumulationCountTexture;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_velocityTexture;
    uint c_prevLinearDepthTexture;
    uint c_prevNormalTexture;
    uint c_outputSHTexture;
    uint c_outputAccumulationCountTexture;
    uint c_bHistoryInvalid;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputTexture = ResourceDescriptorHeap[c_inputSHTexture];
    Texture2D<uint4> historyTexture = ResourceDescriptorHeap[c_historySHTexture];
    Texture2D<uint> historyAccumulationCountTexture = ResourceDescriptorHeap[c_historyAccumulationCountTexture];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
    Texture2D<float> prevLinearDepthTexture = ResourceDescriptorHeap[c_prevLinearDepthTexture];
    Texture2D prevNormalTexture = ResourceDescriptorHeap[c_prevNormalTexture];
    RWTexture2D<uint4> outputTexture = ResourceDescriptorHeap[c_outputSHTexture];
    RWTexture2D<uint> outputAccumulationCountTexture = ResourceDescriptorHeap[c_outputAccumulationCountTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];

    float depth = depthTexture[pos];
    if (depth == 0.0)
    {
        outputTexture[pos] = 0;
        outputAccumulationCountTexture[pos] = 0;
        return;
    }
    
    float3 velocity = velocityTexture[pos].xyz;
    float2 prevUV = GetScreenUV(pos, SceneCB.rcpRenderSize) - velocity.xy * float2(0.5, -0.5);
    float prevDepth = depth - velocity.z;
    if (c_bHistoryInvalid || any(prevUV < 0.0) || any(prevUV > 1.0))
    {
        outputTexture[pos] = inputTexture[pos];
        outputAccumulationCountTexture[pos] = 1;
        return;
    }
    
    float linearDepth = GetLinearDepth(depth);
    float3 N = DecodeNormal(normalTexture[pos].xyz);
    
    Bilinear bilinearFilter = GetBilinearFilter(prevUV, SceneCB.renderSize);
    float2 prevGatherUV = (bilinearFilter.origin + 1.0) * SceneCB.rcpRenderSize;
        
    float4 prevLinearDepth = prevLinearDepthTexture.GatherRed(pointSampler, prevGatherUV).wzxy;

    float3 prevNormal00 = DecodeNormal(prevNormalTexture[int2(bilinearFilter.origin) + int2(0, 0)].xyz);
    float3 prevNormal10 = DecodeNormal(prevNormalTexture[int2(bilinearFilter.origin) + int2(1, 0)].xyz);
    float3 prevNormal01 = DecodeNormal(prevNormalTexture[int2(bilinearFilter.origin) + int2(0, 1)].xyz);
    float3 prevNormal11 = DecodeNormal(prevNormalTexture[int2(bilinearFilter.origin) + int2(1, 1)].xyz);
    
    float4 prevClipPos = float4((prevUV * 2.0 - 1.0) * float2(1.0, -1.0), prevDepth, 1.0);
    float4 prevWorldPos = mul(CameraCB.mtxPrevViewProjectionInverse, prevClipPos);
    
    // Compute disocclusion basing on plane distance
    float3 Xprev = prevWorldPos.xyz / prevWorldPos.w;
    float NoXprev = dot(N, Xprev);
    float NoVprev = NoXprev / GetLinearDepth(prevDepth);
    float4 planeDist = abs(NoVprev * prevLinearDepth - NoXprev);

    const float threshold = 0.01;
    float distToPoint = length(GetWorldPosition(pos, depth) - CameraCB.cameraPos);
    float4 occlusion = step(planeDist, threshold * distToPoint);
    
    const float normalThreshold = 0.9;
    occlusion.x *= dot(N, prevNormal00) > normalThreshold;
    occlusion.y *= dot(N, prevNormal10) > normalThreshold;
    occlusion.z *= dot(N, prevNormal01) > normalThreshold;
    occlusion.w *= dot(N, prevNormal11) > normalThreshold;
    
    float4 bilinearWeights = GetBilinearCustomWeights(bilinearFilter, occlusion);

    SH historySH00 = UnpackSH(historyTexture[int2(bilinearFilter.origin) + int2(0, 0)]);
    SH historySH10 = UnpackSH(historyTexture[int2(bilinearFilter.origin) + int2(1, 0)]);
    SH historySH01 = UnpackSH(historyTexture[int2(bilinearFilter.origin) + int2(0, 1)]);
    SH historySH11 = UnpackSH(historyTexture[int2(bilinearFilter.origin) + int2(1, 1)]);
    SH historySH = ApplyBilinearCustomWeights(historySH00, historySH10, historySH01, historySH11, bilinearWeights);
    
    float4 historyAccumulationCount = (float4)historyAccumulationCountTexture.GatherRed(pointSampler, prevGatherUV).wzxy;
    float accumulationCount = ApplyBilinearCustomWeights(historyAccumulationCount.x, historyAccumulationCount.y, historyAccumulationCount.z, historyAccumulationCount.w, bilinearWeights);
    
    SH inputSH = UnpackSH(inputTexture[pos]);
    SH outputSH = lerp(historySH, inputSH, 1.0 / (1.0 + accumulationCount));
    
    outputTexture[pos] = PackSH(outputSH);
    outputAccumulationCountTexture[pos] = min(accumulationCount + 1, MAX_TEMPORAL_ACCUMULATION_FRAME);
}