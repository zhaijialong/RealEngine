#pragma once

#include "common.hlsli"
#include "model_constants.hlsli"

ModelConstant GetModelConstant(uint address)
{
    return LoadSceneConstantBuffer<ModelInstanceConstant>(address).modelCB;
}

MaterialConstant GetMaterialConstant(uint address)
{
    return LoadSceneConstantBuffer<ModelInstanceConstant>(address).materialCB;
}

ModelConstant GetModelConstant()
{
#if 1
    return LoadSceneConstantBuffer<ModelInstanceConstant>(c_SceneConstantAddress).modelCB;
#else
    return ModelCB;
#endif
}

MaterialConstant GetMaterialConstant()
{
#if 1
    return LoadSceneConstantBuffer<ModelInstanceConstant>(c_SceneConstantAddress).materialCB;
#else
    return MaterialCB;
#endif
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
    uint sceneConstantAddress : COLOR1;
};

VertexOut GetVertex(uint sceneConstantAddress,  uint vertex_id)
{    
    float4 pos = float4(LoadSceneBuffer<float3>(GetModelConstant(sceneConstantAddress).posBufferAddress, vertex_id), 1.0);
    float2 uv = LoadSceneBuffer<float2>(GetModelConstant(sceneConstantAddress).uvBufferAddress, vertex_id);
    float3 normal = LoadSceneBuffer<float3>(GetModelConstant(sceneConstantAddress).normalBufferAddress, vertex_id);

    float4 worldPos = mul(GetModelConstant(sceneConstantAddress).mtxWorld, pos);

    VertexOut v = (VertexOut)0;
    v.pos = mul(CameraCB.mtxViewProjection, worldPos);
    v.uv = uv;
    v.normal = normalize(mul(GetModelConstant(sceneConstantAddress).mtxWorldInverseTranspose, float4(normal, 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //debug::DrawSphere(GetModelConstant(sceneConstantAddress).center, GetModelConstant(sceneConstantAddress).radius, float3(1, 0, 0));
    }
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + v.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    float4 tangent = LoadSceneBuffer<float4>(GetModelConstant(sceneConstantAddress).tangentBufferAddress, vertex_id);
    v.tangent = normalize(mul(GetModelConstant(sceneConstantAddress).mtxWorldInverseTranspose, float4(tangent.xyz, 0.0f)).xyz);
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
#if NON_UNIFORM_RESOURCE
    return ResourceDescriptorHeap[NonUniformResourceIndex(heapIndex)];
#else
    return ResourceDescriptorHeap[heapIndex];
#endif
}

SamplerState GetMaterialSampler()
{
    return SamplerDescriptorHeap[SceneCB.aniso4xSampler];
}

PbrMetallicRoughness GetMaterialMetallicRoughness(VertexOut input)
{
    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = GetMaterialConstant(input.sceneConstantAddress).albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = GetMaterialConstant(input.sceneConstantAddress).metallic;
    pbrMetallicRoughness.roughness = GetMaterialConstant(input.sceneConstantAddress).roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).albedoTexture);
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    pbrMetallicRoughness.albedo *= albedo.xyz;
    pbrMetallicRoughness.alpha = albedo.w;
#endif
    
#if METALLIC_ROUGHNESS_TEXTURE
    Texture2D metallicRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).metallicRoughnessTexture);
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
    pbrSpecularGlossiness.diffuse = GetMaterialConstant(input.sceneConstantAddress).diffuse;
    pbrSpecularGlossiness.specular = GetMaterialConstant(input.sceneConstantAddress).specular;
    pbrSpecularGlossiness.glossiness = GetMaterialConstant(input.sceneConstantAddress).glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
#if DIFFUSE_TEXTURE
    Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).diffuseTexture);
    float4 diffuse = diffuseTexture.Sample(linearSampler, input.uv);
    pbrSpecularGlossiness.diffuse *= diffuse.xyz;
    pbrSpecularGlossiness.alpha = diffuse.w;
#endif
    
#if SPECULAR_GLOSSINESS_TEXTURE
    Texture2D specularGlossinessTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).specularGlossinessTexture);
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

    Texture2D normalTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).normalTexture);
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
    Texture2D aoTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).aoTexture);
    SamplerState linearSampler = GetMaterialSampler();
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif    
    return ao;
}

float3 GetMaterialEmissive(VertexOut input)
{
    float3 emissive = GetMaterialConstant(input.sceneConstantAddress).emissive;
#if EMISSIVE_TEXTURE
    Texture2D emissiveTexture = GetMaterialTexture2D(GetMaterialConstant(input.sceneConstantAddress).emissiveTexture);
    SamplerState linearSampler = GetMaterialSampler();
    emissive *= emissiveTexture.Sample(linearSampler, input.uv).xyz;
#endif
    return emissive;
}


void AlphaTest(float2 uv)
{
    float alpha = 1.0;
    SamplerState linearSampler = GetMaterialSampler();
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant().albedoTexture);
    alpha = albedoTexture.Sample(linearSampler, uv).a;
#elif DIFFUSE_TEXTURE
    Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant().diffuseTexture);
    alpha = diffuseTexture.Sample(linearSampler, uv).a;
#endif
    clip(alpha - GetMaterialConstant().alphaCutoff);
}