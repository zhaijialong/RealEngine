#include "common.hlsli"
#include "gtso.hlsli"

cbuffer CB1 : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_shadowRT;
    uint c_gtaoRT;
    uint c_hdrRT;
};

float Shadow(float3 worldPos, float3 worldNormal, float4x4 mtxLightVP, Texture2D shadowRT, SamplerComparisonState shadowSampler)
{
    float4 shadowPos = mul(mtxLightVP, float4(worldPos + worldNormal * 0.001, 1.0));
    shadowPos /= shadowPos.w;
    shadowPos.xy = GetScreenUV(shadowPos.xy);
    
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
    if (dispatchThreadID.x >= SceneCB.viewWidth || dispatchThreadID.y >= SceneCB.viewHeight)
    {
        return;
    }
    
    int3 pos = int3(dispatchThreadID.xy, 0);
    
    Texture2D depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT.Load(pos).x;
    if(depth == 0.0)
    {
        return;
    }
    
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    
    float4 diffuse = diffuseRT.Load(pos);
    float3 specular = specularRT.Load(pos).xyz;
    float4 normal = normalRT.Load(pos);
    float3 emissive = emissiveRT.Load(pos).xyz;
    
    float3 N = OctNormalDecode(normal.xyz);
    float roughness = normal.w;
    float ao = diffuse.w;
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);
    
    Texture2D shadowRT = ResourceDescriptorHeap[c_shadowRT];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[SceneCB.shadowSampler];
    
    float visibility = Shadow(worldPos, N, SceneCB.mtxLightVP, shadowRT, shadowSampler);
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse.xyz, specular, roughness) * visibility * SceneCB.lightColor;
    
    Texture2D<uint> gtaoRT = ResourceDescriptorHeap[c_gtaoRT];
    
    float gtao;
    float3 bentNormal;
    DecodeVisibilityBentNormal(gtaoRT.Load(pos), gtao, bentNormal);
    
    ao = min(ao, gtao);
    
    float3 indirect_diffuse = diffuse.xyz * float3(0.15, 0.15, 0.2);

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
    
#if GTSO
    bentNormal = normalize(mul(CameraCB.mtxViewInverse, float4(bentNormal, 0.0)).xyz);
    float specularAO = GTSO(R, bentNormal, ao, roughness);
#else
    float specularAO = 1.0;
#endif

    float3 radiance = emissive.xyz + direct_light + indirect_diffuse * ao + indirect_specular * specularAO;
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_hdrRT];    
    outTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}