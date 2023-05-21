#include "noise.hlsli"

cbuffer CB : register(b0)
{
    uint c_seed;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    RWTexture2D<float> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    float width, height;
    outputTexture.GetDimensions(width, height);

    float2 uv = (dispatchThreadID.xy + 0.5) / float2(width, height);
    
    uv *= 2.0;
    uv += float2(fmod(c_seed, 43.5), fmod(c_seed, 63.7));
    
    float f = uber_noise(uv, 12, -1.0, 0.6, 1.0, 0.5, 0.5, 1.8, 0.6).x;
    f = f * 0.5 + 0.1;
    f = pow(f, 1.5);
    
    outputTexture[dispatchThreadID.xy] = f;
}