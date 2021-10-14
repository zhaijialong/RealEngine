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

struct GBufferOutput
{
    float4 albedoRT : SV_TARGET0;
    float4 normalRT : SV_TARGET1;
    float4 emissiveRT : SV_TARGET2;
};

GBufferOutput ps_main(VSOutput input)
{
    float3 N = normalize(input.normal);

    float4 albedo = float4(MaterialCB.albedo.xyz, 1.0);
    float metallic = MaterialCB.metallic;
    float roughness = MaterialCB.roughness;

    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    
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
    
    GBufferOutput output;
    output.albedoRT = float4(albedo.xyz, 1.0); //todo : ao
    output.normalRT = float4(OctNormalEncode(N), roughness, 0.0);
    output.emissiveRT = float4(float3(0.0, 0.0, 0.0), metallic); //todo : emissive
    
    return output;
}
