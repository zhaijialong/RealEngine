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

cbuffer PackRadianceCB : register(b1)
{
    float4 c_hitDistParams;
    uint c_radianceTexture;
    uint c_rayDirectionTexture;
    uint c_linearDepthTexture;
    uint c_outputSH0Texture;
    uint c_outputSH1Texture;
};

[numthreads(8, 8, 1)]
void pack_radiance_hitT(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D radianceTexture = ResourceDescriptorHeap[c_radianceTexture];
    Texture2D<uint> rayDirectionTexture = ResourceDescriptorHeap[c_rayDirectionTexture];
    Texture2D linearDepthTexture = ResourceDescriptorHeap[c_linearDepthTexture];
    RWTexture2D<float4> outputSH0Texture = ResourceDescriptorHeap[c_outputSH0Texture];
    RWTexture2D<float4> outputSH1Texture = ResourceDescriptorHeap[c_outputSH1Texture];

    float3 radiance = radianceTexture[dispatchThreadID.xy].xyz;
    float hitDist = radianceTexture[dispatchThreadID.xy].w;
    float3 rayDirection = DecodeNormal16x2(rayDirectionTexture[dispatchThreadID.xy]);
    float linearDepth = linearDepthTexture[dispatchThreadID.xy].x;

    float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitDist, linearDepth, c_hitDistParams);
    float4 sh1;
    float4 sh0 = REBLUR_FrontEnd_PackSh(radiance, normHitDist, rayDirection, sh1);

    outputSH0Texture[dispatchThreadID.xy] = sh0;
    outputSH1Texture[dispatchThreadID.xy] = sh1;
}

cbuffer ResolveCB : register(b0)
{
    uint c_sh0Texture;
    uint c_sh1Texture;
    uint c_resolveNormalTexture;
    uint c_resolveOutputTexture;
};

[numthreads(8, 8, 1)]
void resolve_output(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D sh0Texture = ResourceDescriptorHeap[c_sh0Texture];
    Texture2D sh1Texture = ResourceDescriptorHeap[c_sh1Texture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_resolveNormalTexture];
    RWTexture2D<float4> resolveOutputTexture = ResourceDescriptorHeap[c_resolveOutputTexture];

    NRD_SG sh = REBLUR_BackEnd_UnpackSh(sh0Texture[dispatchThreadID.xy], sh1Texture[dispatchThreadID.xy]);
    float3 N = DecodeNormal(normalTexture[dispatchThreadID.xy].xyz);

    resolveOutputTexture[dispatchThreadID.xy] = float4(NRD_SH_ResolveDiffuse(sh, N), 0);
}