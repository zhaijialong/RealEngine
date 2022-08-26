#include "../common.hlsli"
#include "../bilinear.hlsli"
#include "sh.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputSHMipsTexture;
    uint c_accumulationCountTexture;
    uint c_outputSHTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputSHMipsTexture = ResourceDescriptorHeap[c_inputSHMipsTexture];
    Texture2D<uint> accumulationCountTexture = ResourceDescriptorHeap[c_accumulationCountTexture];
    RWTexture2D<uint4> outputSHTexture = ResourceDescriptorHeap[c_outputSHTexture];
    
    float2 uv = GetScreenUV(pos, SceneCB.rcpRenderSize);
    Bilinear bilinearFilter = GetBilinearFilter(uv, SceneCB.renderSize);
    
    uint accumulationCount = accumulationCountTexture[pos];
    if (accumulationCount < 10)
    {
        uint2 mipPos = pos / 8;
        
        outputSHTexture[pos] = inputSHMipsTexture.Load(uint3(mipPos, 2));
    }
}