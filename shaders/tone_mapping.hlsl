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
#elif TONY_MC_MAPFACE
    return tony_mc_mapface(color);
#elif AGX
    color = agx(color);
    color = agxLook(color);
    return agxEotf(color);
#endif
}

//https://github.com/EmbarkStudios/kajiya/blob/main/assets/shaders/post_combine.hlsl
float TriangularRemap(float n)
{
    float origin = n * 2.0 - 1.0;
    float v = origin * rsqrt(abs(origin));
    v = max(-1.0, v);
    v -= sign(origin);
    return v;
}

float3 ApplyDither(float3 color, uint2 pos)
{
#if DITHER    
    Texture2D blueNoiseTexture = ResourceDescriptorHeap[SceneCB.blueNoiseTexture];
    float blue_noise = blueNoiseTexture[(pos + SceneCB.frameIndex * uint2(59, 37)) & 255].x;
    float dither = TriangularRemap(blue_noise);
    color += dither / 255.0;
#endif
    return color;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];    
    float3 color = hdrTexture[dispatchThreadID.xy].xyz;

    color = ApplyBloom(color, dispatchThreadID.xy);
    color = ApplyExposure(color);
    color = ApplyToneMapping(color);
    
    color = LinearToSrgb(color.xyz);
    color = ApplyDither(color, dispatchThreadID.xy);
    
    RWTexture2D<float4> ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    ldrTexture[dispatchThreadID.xy] = float4(color, 1.0);
}