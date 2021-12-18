#include "model.hlsli"
#include "debug.hlsli"

VertexOut vs_main(uint vertex_id : SV_VertexID)
{
    VertexOut v = GetVertex(vertex_id);
    return v;
}

struct GBufferOutput
{
    float4 albedoRT : SV_TARGET0;
    float4 normalRT : SV_TARGET1;
    float4 emissiveRT : SV_TARGET2;
};

GBufferOutput ps_main(VertexOut input)
{
    float3 N = input.normal;

    float4 albedo = float4(MaterialCB.albedo.xyz, 1.0);
    float metallic = MaterialCB.metallic;
    float roughness = MaterialCB.roughness;
    float ao = 1.0;
    float3 emissive = MaterialCB.emissive;

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
#if AO_METALLIC_ROUGHNESS_TEXTURE
    ao = metallicRoughness.r;
#endif    
#endif

#if NORMAL_TEXTURE
    float3 T = input.tangent;
    float3 B = input.bitangent;

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
    
#if EMISSIVE_TEXTURE
    Texture2D emissiveTexture = ResourceDescriptorHeap[MaterialCB.emissiveTexture];
    emissive *= emissiveTexture.Sample(linearSampler, input.uv).xyz;
#endif
    
#if AO_TEXTURE && !AO_METALLIC_ROUGHNESS_TEXTURE
    Texture2D aoTexture = ResourceDescriptorHeap[MaterialCB.aoTexture];
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif
    
#define DEBUG_MESHLET 0
#if DEBUG_MESHLET
    uint mhash = Hash(input.meshlet);
    albedo.xyz = float3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
    
    metallic = 0.0;
    roughness = 1.0;
    emissive = float3(0.0, 0.0, 0.0);
#endif
    
    GBufferOutput output;
    output.albedoRT = float4(albedo.xyz, metallic);
    output.normalRT = float4(OctNormalEncode(N), roughness);
    output.emissiveRT = float4(emissive, ao);
    
    return output;
}
