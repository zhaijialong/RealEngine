#include "../common.hlsli"
#include "sh.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputSHTexture;
    uint c_normalTexture;
    uint c_outputSHTexture;
    uint c_outputRadianceTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputSHTexture = ResourceDescriptorHeap[c_inputSHTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<uint4> outputSHTexture = ResourceDescriptorHeap[c_outputSHTexture];
    RWTexture2D<float3> outputRadianceTexture = ResourceDescriptorHeap[c_outputRadianceTexture];

    uint4 packedSH = inputSHTexture[pos];
    SH sh = UnpackSH(packedSH);
    float3 N = DecodeNormal(normalTexture[pos].xyz);
    
    float3 radiance = sh.Unproject(N);

    outputSHTexture[pos] = packedSH;
    outputRadianceTexture[pos] = radiance;
}