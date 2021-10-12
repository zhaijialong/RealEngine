#include "common.hlsli"
#include "model_constants.hlsli"
#include "global_constants.hlsli"

cbuffer VertexCB : register(b0)
{
    uint c_posBuffer;
    uint c_uvBuffer;
    uint c_normalBuffer;
    uint c_tangentBuffer;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
#endif
    float3 worldPos : TEXCOORD1;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[c_posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[c_uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[c_normalBuffer];
    StructuredBuffer<float3> tangentBuffer = ResourceDescriptorHeap[c_tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    
    VSOutput output;
    output.pos = mul(ModelCB.mtxWVP, pos);
    output.uv = uvBuffer[vertex_id];
    output.normal = mul(ModelCB.mtxNormal, float4(normalBuffer[vertex_id], 0.0f)).xyz;
    output.worldPos = mul(ModelCB.mtxWorld, pos).xyz;
    
#if NORMAL_TEXTURE
    output.tangent = mul(ModelCB.mtxWorld, float4(tangentBuffer[vertex_id], 0.0f)).xyz;
#endif
    
    return output;
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

float4 ps_main(VSOutput input) : SV_TARGET
{
    float3 V = normalize(CameraCB.cameraPos - input.worldPos);
    float3 N = normalize(input.normal);

    float4 albedo = float4(MaterialCB.albedo.xyz, 1.0);
    float metallic = MaterialCB.metallic;
    float roughness = MaterialCB.roughness;

    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearRepeatSampler];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
	albedo *= albedoTexture.Sample(linearSampler, input.uv);
#endif
    
#if ALPHA_TEST
    clip(albedo.a - MaterialCB.alphaCutoff);
#endif

#if METALLIC_ROUGHNESS_TEXTURE
    Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[MaterialCB.metallicRoughnessTexture];
    float4 metallicRoughness = metallicRoughnessTexture.Sample(linearSampler, input.uv);
    metallic *= metallicRoughness.b;
    roughness *= metallicRoughness.g;
#endif

#if NORMAL_TEXTURE
    float3 T = normalize(input.tangent);
    float3 B = normalize(cross(T, N));

    Texture2D normalTexture = ResourceDescriptorHeap[MaterialCB.normalTexture];
    float3 normal = normalTexture.Sample(linearSampler, input.uv).xyz;
    
#if RG_NORMAL_TEXTURE
    normal.xy = normal.xy * 2.0 - 1.0;
    normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
#else
    normal = normal * 2.0 - 1.0;
#endif

    N = normalize(normal.x * T + normal.y * B + normal.z * N);
#endif
    
    float3 diffuse = albedo.xyz * (1.0 - metallic);
    float3 specular = lerp(0.04, albedo.xyz, metallic);

    Texture2D shadowRT = ResourceDescriptorHeap[SceneCB.shadowRT];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[SceneCB.shadowSampler];
    
    float visibility = Shadow(input.worldPos, N, SceneCB.mtxLightVP, shadowRT, shadowSampler);
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor;
    
    float3 indirect_diffuse = diffuse * float3(0.15, 0.15, 0.2);
    

    TextureCube envTexture = ResourceDescriptorHeap[SceneCB.envTexture];
    Texture2D brdfTexture = ResourceDescriptorHeap[SceneCB.brdfTexture];
    
    const float MAX_REFLECTION_LOD = 7.0;
    float3 R = reflect(-V, N);
    float3 filtered_env = envTexture.SampleLevel(linearSampler, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    float NdotV = saturate(dot(N, V));
    float2 PreintegratedGF = brdfTexture.Sample(pointSampler, float2(NdotV, roughness)).xy;
    float3 indirect_specular = filtered_env * (specular * PreintegratedGF.x + PreintegratedGF.y);
    
    float3 radiance = direct_light + indirect_diffuse + indirect_specular;

    return float4(radiance, 1.0);
}
