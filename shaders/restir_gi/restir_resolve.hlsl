#include "../common.hlsli"

cbuffer CB : register(b0)
{
    uint c_reservoirTexture;
    uint c_irradianceTexture;
    uint c_rayTexture;
    uint c_normalTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D irradianceTexture = ResourceDescriptorHeap[c_irradianceTexture];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
    Texture2D rayTexture = ResourceDescriptorHeap[c_rayTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float3 normal = DecodeNormal(normalTexture[pos].xyz);
    float3 rayDirection = OctDecode(rayTexture[pos].xy * 2.0 - 1.0);
    float NdotL = saturate(dot(normal, rayDirection));
    
    float3 irradiance = irradianceTexture[pos].xyz;
    float W = reservoirTexture[pos].y;
    outputTexture[pos] = float4(irradiance * NdotL * W, 0.0);
}