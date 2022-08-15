#include "../common.hlsli"

cbuffer CB : register(b0)
{
    uint c_reservoirTexture;
    uint c_radianceTexture;
    uint c_normalTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    uint2 halfPos = pos / 2;
    
    Texture2D radianceTexture = ResourceDescriptorHeap[c_radianceTexture];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    float3 radiance = radianceTexture[halfPos].xyz;
    float W = reservoirTexture[halfPos].y;
    
    outputTexture[pos] = float4(radiance * W, 0.0);
}