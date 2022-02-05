#pragma once

#include "common.hlsli"
#include "model_constants.hlsli"
#include "gpu_scene.hlsli"

namespace model
{

ModelMaterialConstant GetMaterialConstant(uint instance_id)
{
    return LoadSceneConstantBuffer<ModelMaterialConstant>(GetInstanceData(instance_id).materialDataAddress);
}

struct VertexOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
#endif
    
    uint meshlet : COLOR0;
    uint instanceIndex : COLOR1;
};

VertexOut GetVertex(uint instance_id,  uint vertex_id)
{    
    float4 pos = float4(LoadSceneStaticBuffer<float3>(GetInstanceData(instance_id).posBufferAddress, vertex_id), 1.0);
    float2 uv = LoadSceneStaticBuffer<float2>(GetInstanceData(instance_id).uvBufferAddress, vertex_id);
    float3 normal = LoadSceneStaticBuffer<float3>(GetInstanceData(instance_id).normalBufferAddress, vertex_id);

    float4 worldPos = mul(GetInstanceData(instance_id).mtxWorld, pos);

    VertexOut v = (VertexOut)0;
    v.pos = mul(CameraCB.mtxViewProjection, worldPos);
    v.uv = uv;
    v.normal = normalize(mul(GetInstanceData(instance_id).mtxWorldInverseTranspose, float4(normal, 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //debug::DrawSphere(GetInstanceData(instance_id).center, GetInstanceData(instance_id).radius, float3(1, 0, 0));
    }
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    float4 tangent = LoadSceneStaticBuffer<float4>(GetInstanceData(instance_id).tangentBufferAddress, vertex_id);
    v.tangent = normalize(mul(GetInstanceData(instance_id).mtxWorldInverseTranspose, float4(tangent.xyz, 0.0f)).xyz);
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

Texture2D GetMaterialTexture2D(uint heapIndex)
{
#if UNIFORM_RESOURCE
    return ResourceDescriptorHeap[heapIndex];
#else
    return ResourceDescriptorHeap[NonUniformResourceIndex(heapIndex)];
#endif
}

SamplerState GetMaterialSampler()
{
    return SamplerDescriptorHeap[SceneCB.aniso4xSampler];
}

PbrMetallicRoughness GetMaterialMetallicRoughness(VertexOut input)
{
    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = GetMaterialConstant(input.instanceIndex).albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = GetMaterialConstant(input.instanceIndex).metallic;
    pbrMetallicRoughness.roughness = GetMaterialConstant(input.instanceIndex).roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).albedoTexture);
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    pbrMetallicRoughness.albedo *= albedo.xyz;
    pbrMetallicRoughness.alpha = albedo.w;
#endif
    
#if METALLIC_ROUGHNESS_TEXTURE
    Texture2D metallicRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).metallicRoughnessTexture);
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
    pbrSpecularGlossiness.diffuse = GetMaterialConstant(input.instanceIndex).diffuse;
    pbrSpecularGlossiness.specular = GetMaterialConstant(input.instanceIndex).specular;
    pbrSpecularGlossiness.glossiness = GetMaterialConstant(input.instanceIndex).glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
#if DIFFUSE_TEXTURE
    Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).diffuseTexture);
    float4 diffuse = diffuseTexture.Sample(linearSampler, input.uv);
    pbrSpecularGlossiness.diffuse *= diffuse.xyz;
    pbrSpecularGlossiness.alpha = diffuse.w;
#endif
    
#if SPECULAR_GLOSSINESS_TEXTURE
    Texture2D specularGlossinessTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).specularGlossinessTexture);
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

    Texture2D normalTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).normalTexture);
    SamplerState linearSampler = GetMaterialSampler();
    
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
    Texture2D aoTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).aoTexture);
    SamplerState linearSampler = GetMaterialSampler();
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif    
    return ao;
}

float3 GetMaterialEmissive(VertexOut input)
{
    float3 emissive = GetMaterialConstant(input.instanceIndex).emissive;
#if EMISSIVE_TEXTURE
    Texture2D emissiveTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).emissiveTexture);
    SamplerState linearSampler = GetMaterialSampler();
    emissive *= emissiveTexture.Sample(linearSampler, input.uv).xyz;
#endif
    return emissive;
}


void AlphaTest(uint instanceIndex, float2 uv)
{
    float alpha = 1.0;
    SamplerState linearSampler = GetMaterialSampler();
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).albedoTexture);
    alpha = albedoTexture.Sample(linearSampler, uv).a;
#elif DIFFUSE_TEXTURE
    Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).diffuseTexture);
    alpha = diffuseTexture.Sample(linearSampler, uv).a;
#endif
    clip(alpha - GetMaterialConstant(instanceIndex).alphaCutoff);
}

} // namespace model