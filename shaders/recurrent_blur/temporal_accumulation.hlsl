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
    
    float2 velocity = velocityTexture[pos].xy;
    float2 prevUV = GetScreenUV(pos, SceneCB.rcpRenderSize) - velocity * float2(0.5, -0.5);
    if (c_bHistoryInvalid || any(prevUV < 0.0) || any(prevUV > 1.0))
    {
        outputTexture[pos] = inputTexture[pos];
        outputAccumulationCountTexture[pos] = 1;
        return;
    }
    
    Bilinear bilinearFilter = GetBilinearFilter(prevUV, SceneCB.renderSize);
    float2 gatherUV = (bilinearFilter.origin + 1.0) * SceneCB.rcpRenderSize;
    
    uint4 historySH_R = historyTexture.GatherRed(pointSampler, gatherUV);
    uint4 historySH_G = historyTexture.GatherGreen(pointSampler, gatherUV);
    uint4 historySH_B = historyTexture.GatherBlue(pointSampler, gatherUV);
    uint4 historySH_A = historyTexture.GatherAlpha(pointSampler, gatherUV);
    
    uint4 historyAccumulationCount = historyAccumulationCountTexture.GatherRed(pointSampler, gatherUV).wzxy;
    historyAccumulationCount = min(historyAccumulationCount + 1, 32);
    
    SH historySH00 = UnpackSH(uint4(historySH_R.w, historySH_G.w, historySH_B.w, historySH_A.w));
    SH historySH10 = UnpackSH(uint4(historySH_R.z, historySH_G.z, historySH_B.z, historySH_A.z));
    SH historySH01 = UnpackSH(uint4(historySH_R.x, historySH_G.x, historySH_B.x, historySH_A.x));
    SH historySH11 = UnpackSH(uint4(historySH_R.y, historySH_G.y, historySH_B.y, historySH_A.y));
    
    float4 customWeights = 1.0; //todo geometry & normal weight
    float4 bilinearWeights = GetBilinearCustomWeights(bilinearFilter, customWeights);
    
    SH historySH = ApplyBilinearCustomWeights(historySH00, historySH10, historySH01, historySH11, bilinearWeights);
    uint accumulationCount = ApplyBilinearCustomWeights(historyAccumulationCount.x, historyAccumulationCount.y, historyAccumulationCount.z, historyAccumulationCount.w, bilinearWeights);
    
    SH inputSH = UnpackSH(inputTexture[pos]);
    SH outputSH = lerp(historySH, inputSH, 1.0 / (1.0 + accumulationCount));
    
    outputTexture[pos] = PackSH(outputSH);
    outputAccumulationCountTexture[pos] = accumulationCount;
}