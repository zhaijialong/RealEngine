#pragma once

#include "common.hlsli"
#include "model_constants.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
#endif
    
    uint meshlet : COLOR;
};

VertexOut GetVertex(uint vertex_id)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
    StructuredBuffer<float4> tangentBuffer = ResourceDescriptorHeap[ModelCB.tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    float4 worldPos = mul(ModelCB.mtxWorld, pos);

    VertexOut v = (VertexOut)0;
    v.pos = mul(CameraCB.mtxViewProjection, worldPos);
    v.uv = uvBuffer[vertex_id];
    v.normal = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(normalBuffer[vertex_id], 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //debug::DrawSphere(ModelCB.center, ModelCB.radius, float3(1, 0, 0));
    }
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    float4 tangent = tangentBuffer[vertex_id];
    v.tangent = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(tangent.xyz, 0.0f)).xyz);
    v.bitangent = normalize(cross(v.normal, v.tangent) * tangent.w);    
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.tangent * 0.05, float3(1, 0, 0));
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.bitangent * 0.05, float3(0, 1, 0));
#endif
    
    return v;
}


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

float3 GetMaterialNormal(VertexOut input, bool isFrontFace)
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
    
#if DOUBLE_SIDED
    N *= isFrontFace ? 1.0 : -1.0;
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


void AlphaTest(float2 uv)
{
    float alpha = 1.0;
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
    alpha = albedoTexture.Sample(linearSampler, uv).a;
#elif DIFFUSE_TEXTURE
    Texture2D diffuseTexture = ResourceDescriptorHeap[MaterialCB.diffuseTexture];
    alpha = diffuseTexture.Sample(linearSampler, uv).a;
#endif
    clip(alpha - MaterialCB.alphaCutoff);
}