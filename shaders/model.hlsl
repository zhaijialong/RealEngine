#include "model.hlsli"
#include "debug.hlsli"

VertexOut vs_main(uint vertex_id : SV_VertexID)
{
    VertexOut v = GetVertex(vertex_id);
    return v;
}

struct GBufferOutput
{
    float4 diffuseRT : SV_TARGET0;
    float4 specularRT : SV_TARGET1;
    float4 normalRT : SV_TARGET2;
    float3 emissiveRT : SV_TARGET3;
};

struct PbrMetallicRoughness
{
    float3 albedo;
    float alpha;
    float metallic;
    float roughness;
    float ao;
};

struct PbrSpecularGlossiness
{
    float3 diffuse;
    float alpha;
    float3 specular;
    float glossiness;
};

PbrMetallicRoughness GetMaterialMetallicRoughness(VertexOut input)
{
    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = MaterialCB.albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = MaterialCB.metallic;
    pbrMetallicRoughness.roughness = MaterialCB.roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    pbrMetallicRoughness.albedo *= albedo.xyz;
    pbrMetallicRoughness.alpha = albedo.w;
#endif
    
#if METALLIC_ROUGHNESS_TEXTURE
    Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[MaterialCB.metallicRoughnessTexture];
    float4 metallicRoughness = metallicRoughnessTexture.Sample(linearSampler, input.uv);
    pbrMetallicRoughness.metallic *= metallicRoughness.b;
    pbrMetallicRoughness.roughness *= metallicRoughness.g;
    
    #if AO_METALLIC_ROUGHNESS_TEXTURE
        pbrMetallicRoughness.ao = metallicRoughness.r;
    #endif    
#endif //METALLIC_ROUGHNESS_TEXTURE
    
    return pbrMetallicRoughness;
}

PbrSpecularGlossiness GetMaterialSpecularGlossiness(VertexOut input)
{
    PbrSpecularGlossiness pbrSpecularGlossiness;
    pbrSpecularGlossiness.diffuse = MaterialCB.diffuse;
    pbrSpecularGlossiness.specular = MaterialCB.specular;
    pbrSpecularGlossiness.glossiness = MaterialCB.glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    
#if DIFFUSE_TEXTURE
    Texture2D diffuseTexture = ResourceDescriptorHeap[MaterialCB.diffuseTexture];
    float4 diffuse = diffuseTexture.Sample(linearSampler, input.uv);
    pbrSpecularGlossiness.diffuse *= diffuse.xyz;
    pbrSpecularGlossiness.alpha = diffuse.w;
#endif
    
#if SPECULAR_GLOSSINESS_TEXTURE
    Texture2D specularGlossinessTexture = ResourceDescriptorHeap[MaterialCB.specularGlossinessTexture];
    float4 specularGlossiness = specularGlossinessTexture.Sample(linearSampler, input.uv);
    pbrSpecularGlossiness.specular *= specularGlossiness.xyz;
    pbrSpecularGlossiness.glossiness *= specularGlossiness.w;
#endif
    
    return pbrSpecularGlossiness;
}

float3 GetMaterialNormal(VertexOut input)
{
    float3 N = input.normal;
    
#if NORMAL_TEXTURE
    float3 T = input.tangent;
    float3 B = input.bitangent;

    Texture2D normalTexture = ResourceDescriptorHeap[MaterialCB.normalTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    
    float3 normal = normalTexture.Sample(linearSampler, input.uv).xyz;
    
    #if RG_NORMAL_TEXTURE
        normal.xy = normal.xy * 2.0 - 1.0;
        normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
    #else
        normal = normal * 2.0 - 1.0;
    #endif

    N = normalize(normal.x * T + normal.y * B + normal.z * N);
#endif
    
    return N;
}

float GetMaterialAO(VertexOut input)
{
    float ao = 1.0;
    
#if AO_TEXTURE && !AO_METALLIC_ROUGHNESS_TEXTURE
    Texture2D aoTexture = ResourceDescriptorHeap[MaterialCB.aoTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif    
    return ao;
}

float3 GetMaterialEmissive(VertexOut input)
{
    float3 emissive = MaterialCB.emissive;
#if EMISSIVE_TEXTURE
    Texture2D emissiveTexture = ResourceDescriptorHeap[MaterialCB.emissiveTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    emissive *= emissiveTexture.Sample(linearSampler, input.uv).xyz;
#endif
    return emissive;
}


GBufferOutput ps_main(VertexOut input)
{
    PbrMetallicRoughness pbrMetallicRoughness = GetMaterialMetallicRoughness(input);
    PbrSpecularGlossiness pbrSpecularGlossiness = GetMaterialSpecularGlossiness(input);
    float3 N = GetMaterialNormal(input);
    float ao = GetMaterialAO(input);
    float3 emissive = GetMaterialEmissive(input);
    
    float3 diffuse = float3(1, 1, 1);
    float3 specular = float3(0, 0, 0);
    float roughness = 1.0;
    float alpha = 1.0;
    
#if PBR_METALLIC_ROUGHNESS
    diffuse = pbrMetallicRoughness.albedo * (1.0 - pbrMetallicRoughness.metallic);
    specular = lerp(0.04, pbrMetallicRoughness.albedo, pbrMetallicRoughness.metallic);
    roughness = pbrMetallicRoughness.roughness;
    alpha = pbrMetallicRoughness.alpha;
    ao *= pbrMetallicRoughness.ao;
#endif //PBR_METALLIC_ROUGHNESS

#if PBR_SPECULAR_GLOSSINESS
    specular = pbrSpecularGlossiness.specular;
    diffuse = pbrSpecularGlossiness.diffuse * (1.0 - max(max(specular.r, specular.g), specular.b));
    roughness = 1.0 - pbrSpecularGlossiness.glossiness;
    alpha = pbrSpecularGlossiness.alpha;
#endif //PBR_SPECULAR_GLOSSINESS
    
#if ALPHA_TEST
    clip(alpha - MaterialCB.alphaCutoff);
#endif
    
#define DEBUG_MESHLET 0
#if DEBUG_MESHLET
    uint mhash = Hash(input.meshlet);
    diffuse.xyz = float3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
    roughness = 1.0;
    emissive = float3(0.0, 0.0, 0.0);
#endif
    
    GBufferOutput output;
    output.diffuseRT = float4(diffuse, ao);
    output.specularRT = float4(specular, 0);
    output.normalRT = float4(OctNormalEncode(N), roughness);
    output.emissiveRT = emissive;
    
    return output;
}
