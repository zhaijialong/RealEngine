#include "../common.hlsli"
#include "sh.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_normalTexture;
    uint c_historyTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputTexture = ResourceDescriptorHeap[c_inputTexture];
    Texture2D<uint4> historyTexture = ResourceDescriptorHeap[c_historyTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<uint4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    SH inputSH = UnpackSH(inputTexture[pos]);
    SH historySH = UnpackSH(historyTexture[pos]);
    
    SH outputSH = lerp(inputSH, historySH, 0.95);
    
    outputTexture[pos] = PackSH(outputSH);
}