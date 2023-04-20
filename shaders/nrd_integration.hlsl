#include "common.hlsli"
#include "../external/RayTracingDenoiser/Shaders/Include/NRDEncoding.hlsli"
#include "../external/RayTracingDenoiser/Shaders/Include/NRD.hlsli"

cbuffer PackNormalRoughnessCB : register(b0)
{
    uint c_normalTexture;
    uint c_packedNormalTexture;
};

[numthreads(8, 8, 1)]
void pack_normal_roughness(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<float4> packedNormalTexture = ResourceDescriptorHeap[c_packedNormalTexture];

    float4 normal = normalTexture[dispatchThreadID.xy];
    float3 N = DecodeNormal(normal.xyz);
    float roughness = normal.w;

    packedNormalTexture[dispatchThreadID.xy] = NRD_FrontEnd_PackNormalAndRoughness(N, roughness);
}

cbuffer PackRadianceCB : register(b0)
{
    float4 c_hitDistParams;
    uint c_radianceTexture;
    uint c_linearDepthTexture;
    uint c_packedRadianceTexture;
};

[numthreads(8, 8, 1)]
void pack_radiance_hitT(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D radianceTexture = ResourceDescriptorHeap[c_radianceTexture];
    Texture2D linearDepthTexture = ResourceDescriptorHeap[c_linearDepthTexture];
    RWTexture2D<float4> packedRadianceTexture = ResourceDescriptorHeap[c_packedRadianceTexture];

    float3 radiance = radianceTexture[dispatchThreadID.xy].xyz;
    float hitDist = radianceTexture[dispatchThreadID.xy].w;
    float linearDepth = linearDepthTexture[dispatchThreadID.xy].x;

    float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitDist, linearDepth, c_hitDistParams);
    packedRadianceTexture[dispatchThreadID.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(radiance, normHitDist);
}

cbuffer ResolveCB : register(b0)
{
    uint c_denoiserOutput;
    uint c_resolveOutput;
};

[numthreads(8, 8, 1)]
void resolve_output(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D denoiserOutput = ResourceDescriptorHeap[c_denoiserOutput];
    RWTexture2D<float4> resolveOutput = ResourceDescriptorHeap[c_resolveOutput];

    resolveOutput[dispatchThreadID.xy] = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(denoiserOutput[dispatchThreadID.xy]);
}