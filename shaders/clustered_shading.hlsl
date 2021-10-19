#include "common.hlsli"
#include "global_constants.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_width;
    uint c_height;
    float c_invWidth;
    float c_invHeight;
};

cbuffer CB1 : register(b1)
{
    uint c_albedoRT;
    uint c_normalRT;
    uint c_emissiveRT;
    uint c_depthRT;
    
    uint c_shadowRT;
    uint c_hdrRT;
    float2 c_zparams;
    
    float4x4 c_mtxInvViewProj;
};

float3 GetWorldPosition(uint2 pos, float depth)
{
    float2 uv = ((float2)pos + 0.5) * float2(c_invWidth, c_invHeight);
    float4 clipPos = float4((uv * 2.0 - 1.0) * float2(1.0, -1.0), depth, 1.0);
    float4 worldPos = mul(c_mtxInvViewProj, clipPos);
    worldPos.xyz /= worldPos.w;
    
    return worldPos.xyz;
}

float Shadow(float3 worldPos, float3 worldNormal, float4x4 mtxLightVP, Texture2D shadowRT, SamplerComparisonState shadowSampler)
{
    float4 shadowPos = mul(mtxLightVP, float4(worldPos + worldNormal * 0.001, 1.0));
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5, -0.5) + 0.5;
    
    const float halfTexel = 0.5 / 4096;
    float visibility = shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(halfTexel, halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(-halfTexel, halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(halfTexel, -halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(-halfTexel, -halfTexel), shadowPos.z).x;

    return visibility / 4.0f;
}


[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_width || dispatchThreadID.y >= c_height)
    {
        return;
    }
    
    int3 pos = int3(dispatchThreadID.xy, 0);
    
    Texture2D depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT.Load(pos).x;
    //float linear_depth = 1.0f / (depth * c_zparams.x - c_zparams.y);
    if(depth == 0.0)
    {
        return;
    }
    
    Texture2D albedoRT = ResourceDescriptorHeap[c_albedoRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    
    float4 albedo = albedoRT.Load(pos);
    float4 normal = normalRT.Load(pos);
    float4 emissive = emissiveRT.Load(pos);
    
    float3 N = OctNormalDecode(normal.xy);
    float ao = albedo.w;
    float roughness = normal.z;
    float metallic = emissive.w;
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);

    float3 diffuse = albedo.xyz * (1.0 - metallic);
    float3 specular = lerp(0.04, albedo.xyz, metallic);
    
    Texture2D shadowRT = ResourceDescriptorHeap[c_shadowRT];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[SceneCB.shadowSampler];
    
    float visibility = Shadow(worldPos, N, SceneCB.mtxLightVP, shadowRT, shadowSampler);
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor;
    
    float3 indirect_diffuse = diffuse * float3(0.15, 0.15, 0.2);

    TextureCube envTexture = ResourceDescriptorHeap[SceneCB.envTexture];
    Texture2D brdfTexture = ResourceDescriptorHeap[SceneCB.brdfTexture];
    
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    
    const float MAX_REFLECTION_LOD = 7.0;
    float3 R = reflect(-V, N);
    float3 filtered_env = envTexture.SampleLevel(linearSampler, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    float NdotV = saturate(dot(N, V));
    float2 PreintegratedGF = brdfTexture.Sample(pointSampler, float2(NdotV, roughness)).xy;
    float3 indirect_specular = filtered_env * (specular * PreintegratedGF.x + PreintegratedGF.y);
    
    float3 radiance = emissive.xyz + direct_light + indirect_diffuse * ao + indirect_specular;
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_hdrRT];    
    outTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}