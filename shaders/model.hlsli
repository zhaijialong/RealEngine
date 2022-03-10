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
    
    float3 worldPos : WORLD_POSITION;
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
    output.worldPos = worldPos.xyz;
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

struct Primitive
{
    uint instanceID;
    uint primitiveIndex;

    static Primitive Create(uint instanceID, uint primitiveIndex)
    {
        Primitive primitive;
        primitive.instanceID = instanceID;
        primitive.primitiveIndex = primitiveIndex;
        return primitive;
    }
    
    float2 GetUV(float3 barycentricCoordinates)
    {
        uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveIndex);
        
        Vertex v0 = GetVertex(instanceID, primitiveIndices.x);
        Vertex v1 = GetVertex(instanceID, primitiveIndices.y);
        Vertex v2 = GetVertex(instanceID, primitiveIndices.z);

        return v0.uv * barycentricCoordinates.x + v1.uv * barycentricCoordinates.y + v2.uv * barycentricCoordinates.z;
    }

    float3 GetNormal(float3 barycentricCoordinates)
    {
        uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveIndex);
        
        Vertex v0 = GetVertex(instanceID, primitiveIndices.x);
        Vertex v1 = GetVertex(instanceID, primitiveIndices.y);
        Vertex v2 = GetVertex(instanceID, primitiveIndices.z);

        return v0.normal * barycentricCoordinates.x + v1.normal * barycentricCoordinates.y + v2.normal * barycentricCoordinates.z;
    }

    float4 GetTangent(float3 barycentricCoordinates)
    {
        uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveIndex);
        
        Vertex v0 = GetVertex(instanceID, primitiveIndices.x);
        Vertex v1 = GetVertex(instanceID, primitiveIndices.y);
        Vertex v2 = GetVertex(instanceID, primitiveIndices.z);

        return v0.tangent * barycentricCoordinates.x + v1.tangent * barycentricCoordinates.y + v2.tangent * barycentricCoordinates.z;
    }
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
    return SamplerDescriptorHeap[SceneCB.aniso8xSampler];
}

bool IsAlbedoTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).albedoTexture != INVALID_RESOURCE_INDEX;
#else
    #if ALBEDO_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsMetallicRoughnessTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).metallicRoughnessTexture != INVALID_RESOURCE_INDEX;
#else
    #if METALLIC_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsAOMetallicRoughnessTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).metallicRoughnessTexture != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).metallicRoughnessTexture == GetMaterialConstant(instanceIndex).aoTexture;
#else
    #if AO_METALLIC_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

PbrMetallicRoughness GetMaterialMetallicRoughness(uint instanceIndex, float2 uv)
{
    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = GetMaterialConstant(instanceIndex).albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = GetMaterialConstant(instanceIndex).metallic;
    pbrMetallicRoughness.roughness = GetMaterialConstant(instanceIndex).roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
    if(IsAlbedoTextureEnabled(instanceIndex))
    {
        Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).albedoTexture);
        float4 albedo = albedoTexture.Sample(linearSampler, uv);
        pbrMetallicRoughness.albedo *= albedo.xyz;
        pbrMetallicRoughness.alpha = albedo.w;
    }
    
    if(IsMetallicRoughnessTextureEnabled(instanceIndex))
    {
        Texture2D metallicRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).metallicRoughnessTexture);
        float4 metallicRoughness = metallicRoughnessTexture.Sample(linearSampler, uv);
        pbrMetallicRoughness.metallic *= metallicRoughness.b;
        pbrMetallicRoughness.roughness *= metallicRoughness.g;

        if(IsAOMetallicRoughnessTextureEnabled(instanceIndex))
        {
            pbrMetallicRoughness.ao = metallicRoughness.r;
        }
    }
    
    return pbrMetallicRoughness;
}

bool IsDiffuseTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).diffuseTexture != INVALID_RESOURCE_INDEX;
#else
    #if DIFFUSE_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsSpecularGlossinessTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).specularGlossinessTexture != INVALID_RESOURCE_INDEX;
#else
    #if SPECULAR_GLOSSINESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

PbrSpecularGlossiness GetMaterialSpecularGlossiness(uint instanceIndex, float2 uv)
{
    PbrSpecularGlossiness pbrSpecularGlossiness;
    pbrSpecularGlossiness.diffuse = GetMaterialConstant(instanceIndex).diffuse;
    pbrSpecularGlossiness.specular = GetMaterialConstant(instanceIndex).specular;
    pbrSpecularGlossiness.glossiness = GetMaterialConstant(instanceIndex).glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    SamplerState linearSampler = GetMaterialSampler();
    
    if(IsDiffuseTextureEnabled(instanceIndex))
    {
        Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).diffuseTexture);
        float4 diffuse = diffuseTexture.Sample(linearSampler, uv);
        pbrSpecularGlossiness.diffuse *= diffuse.xyz;
        pbrSpecularGlossiness.alpha = diffuse.w;
    }
    
    if(IsSpecularGlossinessTextureEnabled(instanceIndex))
    {
        Texture2D specularGlossinessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).specularGlossinessTexture);
        float4 specularGlossiness = specularGlossinessTexture.Sample(linearSampler, uv);
        pbrSpecularGlossiness.specular *= specularGlossiness.xyz;
        pbrSpecularGlossiness.glossiness *= specularGlossiness.w;
    }
    
    return pbrSpecularGlossiness;
}

bool IsNormalTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).normalTexture != INVALID_RESOURCE_INDEX;
#else
    #if NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsRGNormalTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).bRGNormalTexture;
#else
    #if RG_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float3 GetMaterialNormal(uint instanceIndex, float2 uv, float3 T, float3 B, float3 N)
{
    Texture2D normalTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).normalTexture);
    SamplerState linearSampler = GetMaterialSampler();
    
    float3 normal = normalTexture.Sample(linearSampler, uv).xyz;
    
    if(IsRGNormalTextureEnabled(instanceIndex))
    {
        normal.xy = normal.xy * 2.0 - 1.0;
        normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
    }
    else
    {
        normal = normal * 2.0 - 1.0;
    }

    N = normalize(normal.x * T + normal.y * B + normal.z * N);  

    return N;
}

bool IsAOTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).aoTexture != INVALID_RESOURCE_INDEX;
#else
    #if AO_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float GetMaterialAO(uint instanceIndex, float2 uv)
{
    float ao = 1.0;
    
    if(IsAOTextureEnabled(instanceIndex) && !IsAOMetallicRoughnessTextureEnabled(instanceIndex))
    {
        Texture2D aoTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).aoTexture);
        SamplerState linearSampler = GetMaterialSampler();
        ao = aoTexture.Sample(linearSampler, uv).x;
    }

    return ao;
}

bool IsEmissiveTextureEnabled(uint instanceIndex)
{
#ifdef DYNAMIC_MATERIAL_SWITCH
    return GetMaterialConstant(instanceIndex).emissiveTexture != INVALID_RESOURCE_INDEX;
#else
    #if EMISSIVE_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float3 GetMaterialEmissive(uint instanceIndex, float2 uv)
{
    float3 emissive = GetMaterialConstant(instanceIndex).emissive;
    if(IsEmissiveTextureEnabled(instanceIndex))
    {
        Texture2D emissiveTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).emissiveTexture);
        SamplerState linearSampler = GetMaterialSampler();
        emissive *= emissiveTexture.Sample(linearSampler, uv).xyz;
    }
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