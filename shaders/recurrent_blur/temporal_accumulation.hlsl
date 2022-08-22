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
    
    uint4 historySH_R = historyTexture.GatherRed(pointSampler, prevGatherUV);
    uint4 historySH_G = historyTexture.GatherGreen(pointSampler, prevGatherUV);
    uint4 historySH_B = historyTexture.GatherBlue(pointSampler, prevGatherUV);
    uint4 historySH_A = historyTexture.GatherAlpha(pointSampler, prevGatherUV);    
    float4 historyAccumulationCount = (float4)historyAccumulationCountTexture.GatherRed(pointSampler, prevGatherUV).wzxy;
    
    SH historySH00 = UnpackSH(uint4(historySH_R.w, historySH_G.w, historySH_B.w, historySH_A.w));
    SH historySH10 = UnpackSH(uint4(historySH_R.z, historySH_G.z, historySH_B.z, historySH_A.z));
    SH historySH01 = UnpackSH(uint4(historySH_R.x, historySH_G.x, historySH_B.x, historySH_A.x));
    SH historySH11 = UnpackSH(uint4(historySH_R.y, historySH_G.y, historySH_B.y, historySH_A.y));
    
    float4 prevLinearDepth = prevLinearDepthTexture.GatherRed(pointSampler, prevGatherUV).wzxy;
    float4 prevNormal_R = prevNormalTexture.GatherRed(pointSampler, prevGatherUV);
    float4 prevNormal_G = prevNormalTexture.GatherGreen(pointSampler, prevGatherUV);
    float4 prevNormal_B = prevNormalTexture.GatherBlue(pointSampler, prevGatherUV);

    float3 prevNormal00 = DecodeNormal(float3(prevNormal_R.w, prevNormal_G.w, prevNormal_B.w));
    float3 prevNormal10 = DecodeNormal(float3(prevNormal_R.z, prevNormal_G.z, prevNormal_B.z));
    float3 prevNormal01 = DecodeNormal(float3(prevNormal_R.x, prevNormal_G.x, prevNormal_B.x));
    float3 prevNormal11 = DecodeNormal(float3(prevNormal_R.y, prevNormal_G.y, prevNormal_B.y));
    
    float4 customWeights; //todo : need to be improved
    customWeights.x = abs(prevLinearDepth.x - linearDepth) < 0.1 && saturate(dot(prevNormal00, N)) > 0.9;
    customWeights.y = abs(prevLinearDepth.y - linearDepth) < 0.1 && saturate(dot(prevNormal10, N)) > 0.9;
    customWeights.z = abs(prevLinearDepth.z - linearDepth) < 0.1 && saturate(dot(prevNormal01, N)) > 0.9;
    customWeights.w = abs(prevLinearDepth.w - linearDepth) < 0.1 && saturate(dot(prevNormal11, N)) > 0.9;
    float4 bilinearWeights = GetBilinearCustomWeights(bilinearFilter, customWeights);
    
    SH historySH = ApplyBilinearCustomWeights(historySH00, historySH10, historySH01, historySH11, bilinearWeights);
    float accumulationCount = ApplyBilinearCustomWeights(historyAccumulationCount.x, historyAccumulationCount.y, historyAccumulationCount.z, historyAccumulationCount.w, bilinearWeights);
    
    SH inputSH = UnpackSH(inputTexture[pos]);
    SH outputSH = lerp(historySH, inputSH, 1.0 / (1.0 + accumulationCount));
    
    outputTexture[pos] = PackSH(outputSH);
    outputAccumulationCountTexture[pos] = min(accumulationCount + 1, 32);
}