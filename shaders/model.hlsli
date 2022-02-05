#pragma once

#include "common.hlsli"
#include "model_constants.hlsli"
#include "gpu_scene.hlsli"
#include "debug.hlsli"

namespace model
{

ModelMaterialConstant GetMaterialConstant(uint instance_id)
{
    return LoadSceneConstantBuffer<ModelMaterialConstant>(GetInstanceData(instance_id).materialDataAddress);
}

struct Vertex
{
    float3 pos;
    float2 uv;
    float3 normal;
    float4 tangent;
};

struct VertexOutput
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

Vertex GetVertex(uint instance_id,  uint vertex_id)
{
    InstanceData instanceData = GetInstanceData(instance_id);

    Vertex v;
    v.uv = LoadSceneStaticBuffer<float2>(instanceData.uvBufferAddress, vertex_id);

    if(instanceData.bVertexAnimation)
    {
        v.pos = LoadSceneAnimationBuffer<float3>(instanceData.posBufferAddress, vertex_id);
        v.normal = LoadSceneAnimationBuffer<float3>(instanceData.normalBufferAddress, vertex_id);
        v.tangent = LoadSceneAnimationBuffer<float4>(instanceData.tangentBufferAddress, vertex_id);
    }
    else
    {
        v.pos = LoadSceneStaticBuffer<float3>(instanceData.posBufferAddress, vertex_id);
        v.normal = LoadSceneStaticBuffer<float3>(instanceData.normalBufferAddress, vertex_id);
        v.tangent = LoadSceneStaticBuffer<float4>(instanceData.tangentBufferAddress, vertex_id);
    }

    return v;
}

VertexOutput GetVertexOutput(uint instance_id,  uint vertex_id)
{
    InstanceData instanceData = GetInstanceData(instance_id);
    Vertex v = GetVertex(instance_id, vertex_id);

    float4 worldPos = mul(instanceData.mtxWorld, float4(v.pos, 1.0));

    VertexOutput output = (VertexOutput)0;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);
    output.uv = v.uv;
    output.normal = normalize(mul(instanceData.mtxWorldInverseTranspose, float4(v.normal, 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //debug::DrawSphere(instanceData.center, instanceData.radius, float3(1, 0, 0));
    }
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + output.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    output.tangent = normalize(mul(instanceData.mtxWorldInverseTranspose, float4(v.tangent.xyz, 0.0f)).xyz);
    output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);    
    
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + output.tangent * 0.05, float3(1, 0, 0));
    //debug::DrawLine(worldPos.xyz, worldPos.xyz + output.bitangent * 0.05, float3(0, 1, 0));
#endif
    
    return output;
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

PbrMetallicRoughness GetMaterialMetallicRoughness(VertexOutput input)
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

PbrSpecularGlossiness GetMaterialSpecularGlossiness(VertexOutput input)
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

float3 GetMaterialNormal(VertexOutput input, bool isFrontFace)
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

float GetMaterialAO(VertexOutput input)
{
    float ao = 1.0;
    
#if AO_TEXTURE && !AO_METALLIC_ROUGHNESS_TEXTURE
    Texture2D aoTexture = GetMaterialTexture2D(GetMaterialConstant(input.instanceIndex).aoTexture);
    SamplerState linearSampler = GetMaterialSampler();
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif    
    return ao;
}

float3 GetMaterialEmissive(VertexOutput input)
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