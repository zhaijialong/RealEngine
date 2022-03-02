#include "tone_mapping.hlsli"
#include "bloom.hlsli"

cbuffer CB : register(b1)
{
    uint c_hdrTexture;
    uint c_ldrTexture;
    uint c_exposureTexture;
    uint c_bloomTexture;

    float2 c_pixelSize;
    float2 c_bloomPixelSize;

    float c_bloomIntensity;
};

float3 ApplyBloom(float3 color, uint2 pos)
{
#if BLOOM
    Texture2D bloomTexture = ResourceDescriptorHeap[c_bloomTexture];
    float2 uv = ((float2)pos + 0.5) * c_pixelSize;
    float3 bloom = BloomUpsample(bloomTexture, uv, c_bloomPixelSize);
    color += bloom * c_bloomIntensity;
#endif
    
    return color;
}

float3 ApplyExposure(float3 color)
{
    Texture2D<float> exposureTexture = ResourceDescriptorHeap[c_exposureTexture];
    float exposure = exposureTexture.Load(uint3(0, 0, 0));

    return color * exposure;
}

float3 ApplyToneMapping(float3 color)
{
#if ACES
    return ACESFitted(color);
#elif NEUTRAL
    return neutral_tonemap(color);
#endif
}

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];    
    float3 color = hdrTexture[dispatchThreadID.xy].xyz;

    color = ApplyBloom(color, dispatchThreadID.xy);
    color = ApplyExposure(color);
    color = ApplyToneMapping(color);
    
    RWTexture2D<float4> ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    ldrTexture[dispatchThreadID.xy] = float4(LinearToSrgb(color.xyz), 1.0);
}