#include "common.hlsli"

cbuffer CB : register(b0)
{
    uint c_depth;
    uint c_normal;
    uint c_output;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    static const uint2 offsets[4] =
    {
        uint2(1, 1),
        uint2(1, 0),
        uint2(0, 0),
        uint2(0, 1),
    };
    
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depth];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normal];
    RWTexture2D<uint2> outputTexture = ResourceDescriptorHeap[c_output];

    uint2 pos = dispatchThreadID.xy * 2 + offsets[SceneCB.frameIndex % 4];
    float depth = depthTexture[pos];
    float3 normal = DecodeNormal(normalTexture[pos].xyz);

    outputTexture[dispatchThreadID.xy] = uint2(asuint(depth), EncodeNormal16x2(normal));
}