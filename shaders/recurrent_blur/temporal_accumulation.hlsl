#include "sh.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_historyTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    Texture2D historyTexture = ResourceDescriptorHeap[c_historyTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    
    outputTexture[pos] = lerp(inputTexture[pos], historyTexture[pos], 0.95);
}