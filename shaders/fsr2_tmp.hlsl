#include "common.hlsli"

cbuffer CB : register(b0)
{
    uint c_input;
    uint c_output;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 uv = GetScreenUV(dispatchThreadID.xy, SceneCB.rcpDisplaySize);

    Texture2D input = ResourceDescriptorHeap[c_input];
    RWTexture2D<float4> output = ResourceDescriptorHeap[c_output];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    
    output[dispatchThreadID.xy] = input.SampleLevel(linearSampler, uv, 0);
}