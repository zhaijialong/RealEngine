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
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    
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
    output.tangent = normalize(mul(instanceData.mtxWorldInverseTranspose, float4(v.tangent.xyz, 0.0f)).xyz);
    output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);  
    
    if (vertex_id == 0 && instanceData.bShowBoundingSphere)
    {
        debug::DrawSphere(instanceData.center, instanceData.radius, float3(1, 0, 0));
    }
    
    if (instanceData.bShowTangent)
    {
        debug::DrawLine(worldPos.xyz, worldPos.xyz + output.tangent * 0.05, float3(1, 0, 0));
    }
        
    if(instanceData.bShowBitangent)
    {
        debug::DrawLine(worldPos.xyz, worldPos.xyz + output.bitangent * 0.05, float3(0, 1, 0));
    }
        
    if(instanceData.bShowNormal)
    {
        debug::DrawLine(worldPos.xyz, worldPos.xyz + output.normal * 0.05, float3(0, 0, 1));
    }

    return output;
}

struct Primitive
{
    uint instanceID;
    uint primitiveIndex;
    Vertex v0;
    Vertex v1;
    Vertex v2;
    float lodConstant;

    static Primitive Create(uint instanceID, uint primitiveIndex)
    {
        Primitive primitive;
        primitive.instanceID = instanceID;
        primitive.primitiveIndex = primitiveIndex;
            
        uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveIndex);
        primitive.v0 = GetVertex(instanceID, primitiveIndices.x);
        primitive.v1 = GetVertex(instanceID, primitiveIndices.y);
        primitive.v2 = GetVertex(instanceID, primitiveIndices.z);
        
        InstanceData instanceData = GetInstanceData(instanceID);
        float3 p0 = mul(instanceData.mtxWorld, float4(primitive.v0.pos, 1.0)).xyz;
        float3 p1 = mul(instanceData.mtxWorld, float4(primitive.v1.pos, 1.0)).xyz;
        float3 p2 = mul(instanceData.mtxWorld, float4(primitive.v2.pos, 1.0)).xyz;
        float2 uv0 = primitive.v0.uv;
        float2 uv1 = primitive.v1.uv;
        float2 uv2 = primitive.v2.uv;
        primitive.lodConstant = ComputeTriangleLODConstant(p0, p1, p2, uv0, uv1, uv2);
            
        return primitive;
    }
    
    float2 GetUV(float3 barycentricCoordinates)
    {
        return v0.uv * barycentricCoordinates.x + v1.uv * barycentricCoordinates.y + v2.uv * barycentricCoordinates.z;
    }

    float3 GetNormal(float3 barycentricCoordinates)
    {
        return v0.normal * barycentricCoordinates.x + v1.normal * barycentricCoordinates.y + v2.normal * barycentricCoordinates.z;
    }

    float4 GetTangent(float3 barycentricCoordinates)
    {
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
#ifdef RAY_TRACING
    return SamplerDescriptorHeap[SceneCB.bilinearRepeatSampler];
#else
    return SamplerDescriptorHeap[SceneCB.aniso8xSampler];
#endif
}
    
float4 SampleMaterialTexture(MaterialTextureInfo textureInfo, float2 uv, float mipLOD)
{
    Texture2D texture = GetMaterialTexture2D(textureInfo.index);
    SamplerState linearSampler = GetMaterialSampler();
    uv = textureInfo.TransformUV(uv);

#ifdef RAY_TRACING
    return texture.SampleLevel(linearSampler, uv, mipLOD);
#else
    return texture.Sample(linearSampler, uv);
#endif
}

bool IsAlbedoTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).albedoTexture.index != INVALID_RESOURCE_INDEX;
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).metallicRoughnessTexture.index != INVALID_RESOURCE_INDEX;
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).metallicRoughnessTexture.index != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).metallicRoughnessTexture.index == GetMaterialConstant(instanceIndex).aoTexture.index;
#else
    #if AO_METALLIC_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

PbrMetallicRoughness GetMaterialMetallicRoughness(uint instanceIndex, float2 uv, float albedoMipLOD = 0.0, float metallicMipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);

    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = material.albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = material.metallic;
    pbrMetallicRoughness.roughness = material.roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    if(IsAlbedoTextureEnabled(instanceIndex))
    {
        float4 albedo = SampleMaterialTexture(material.albedoTexture, uv, albedoMipLOD);
        pbrMetallicRoughness.albedo *= albedo.xyz;
        pbrMetallicRoughness.alpha = albedo.w;
    }
    
    if(IsMetallicRoughnessTextureEnabled(instanceIndex))
    {
        float4 metallicRoughness = SampleMaterialTexture(material.metallicRoughnessTexture, uv, metallicMipLOD);
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).diffuseTexture.index != INVALID_RESOURCE_INDEX;
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).specularGlossinessTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if SPECULAR_GLOSSINESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

PbrSpecularGlossiness GetMaterialSpecularGlossiness(uint instanceIndex, float2 uv, float diffuseMipLOD = 0.0, float specularMipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
        
    PbrSpecularGlossiness pbrSpecularGlossiness;
    pbrSpecularGlossiness.diffuse = material.diffuse;
    pbrSpecularGlossiness.specular = material.specular;
    pbrSpecularGlossiness.glossiness = material.glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    if(IsDiffuseTextureEnabled(instanceIndex))
    {
        float4 diffuse = SampleMaterialTexture(material.diffuseTexture, uv, diffuseMipLOD);
        pbrSpecularGlossiness.diffuse *= diffuse.xyz;
        pbrSpecularGlossiness.alpha = diffuse.w;
    }
    
    if(IsSpecularGlossinessTextureEnabled(instanceIndex))
    {
        float4 specularGlossiness = SampleMaterialTexture(material.specularGlossinessTexture, uv, specularMipLOD);
        pbrSpecularGlossiness.specular *= specularGlossiness.xyz;
        pbrSpecularGlossiness.glossiness *= specularGlossiness.w;
    }
    
    return pbrSpecularGlossiness;
}

bool IsNormalTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).normalTexture.index != INVALID_RESOURCE_INDEX;
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).bRGNormalTexture;
#else
    #if RG_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float3 GetMaterialNormal(uint instanceIndex, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0f)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);    
    float3 normal = SampleMaterialTexture(material.normalTexture, uv, mipLOD).xyz;
    
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).aoTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if AO_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float GetMaterialAO(uint instanceIndex, float2 uv, float mipLOD = 0.0)
{
    float ao = 1.0;
    
    if(IsAOTextureEnabled(instanceIndex) && !IsAOMetallicRoughnessTextureEnabled(instanceIndex))
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
        ao = SampleMaterialTexture(material.aoTexture, uv, mipLOD).x;
    }

    return ao;
}

bool IsEmissiveTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).emissiveTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if EMISSIVE_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float3 GetMaterialEmissive(uint instanceIndex, float2 uv, float mipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
    float3 emissive = material.emissive;
    if(IsEmissiveTextureEnabled(instanceIndex))
    {
        emissive *= SampleMaterialTexture(material.emissiveTexture, uv, mipLOD).xyz;
    }
    return emissive;
}
    
bool IsAnisotropyTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).anisotropyTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if ANISOTROPIC_TANGENT_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}
    
float3 GetMaterialAnisotropyTangent(uint instanceIndex, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0)
{
     float3 anisotropicT = T;
    if(IsAnisotropyTextureEnabled(instanceIndex))
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
        float2 anisotropy = SampleMaterialTexture(material.anisotropyTexture, uv, mipLOD).xy * 2.0 - 1.0;
        anisotropicT = T * anisotropy.x + B * anisotropy.y;
    }

    anisotropicT = normalize(anisotropicT - dot(anisotropicT, N) * N); // reproject on normal plane

    return anisotropicT;
}

bool IsSheenColorTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).sheenColorTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if SHEEN_COLOR_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsSheenColorRoughnessTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).sheenColorTexture.index != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).sheenColorTexture.index == GetMaterialConstant(instanceIndex).sheenRoughnessTexture.index;
#else
    #if SHEEN_COLOR_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsSheenRoughnessTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).sheenRoughnessTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if SHEEN_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float4 GetMaterialSheenColorAndRoughness(uint instanceIndex, float2 uv, float mipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
    float3 sheenColor = material.sheenColor;
    float sheenRoughness = material.sheenRoughness;
    
    if(IsSheenColorTextureEnabled(instanceIndex))
    {
        float4 sheenTexture = SampleMaterialTexture(material.sheenColorTexture, uv, mipLOD);
        sheenColor *= sheenTexture.xyz;
            
        if (IsSheenColorRoughnessTextureEnabled(instanceIndex))
        {
            sheenRoughness *= sheenTexture.w;
        }
    }
        
    if (IsSheenRoughnessTextureEnabled(instanceIndex) && !IsSheenColorRoughnessTextureEnabled(instanceIndex))
    {
        sheenRoughness *= SampleMaterialTexture(material.sheenRoughnessTexture, uv, mipLOD).w;
    }

    return float4(sheenColor, sheenRoughness);
}

bool IsClearCoatTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsClearCoatRoughnessCombinedTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatTexture.index != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).clearCoatTexture.index == GetMaterialConstant(instanceIndex).clearCoatRoughnessTexture.index;
#else
    #if CLEAR_COAT_ROUGHNESS_COMBINED_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsClearCoatRoughnessTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatRoughnessTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float2 GetMaterialClearCoatAndRoughness(uint instanceIndex, float2 uv, float mipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
    float clearCoat = material.clearCoat;
    float clearCoatRoughness = material.clearCoatRoughness;

    if(IsClearCoatTextureEnabled(instanceIndex))
    {
        float4 clearCoatTex = SampleMaterialTexture(material.clearCoatTexture, uv, mipLOD);
        clearCoat *= clearCoatTex.r;
            
        if (IsClearCoatRoughnessCombinedTextureEnabled(instanceIndex))
        {
            clearCoatRoughness *= clearCoatTex.g;
        }
    }

    if (IsClearCoatRoughnessTextureEnabled(instanceIndex) && !IsClearCoatRoughnessCombinedTextureEnabled(instanceIndex))
    {
        clearCoatRoughness *= SampleMaterialTexture(material.clearCoatRoughnessTexture, uv, mipLOD).g;
    }
        
    return float2(clearCoat, clearCoatRoughness);
}

bool IsClearCoatNormalTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatNormalTexture.index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

bool IsRGClearCoatNormalTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).bRGClearCoatNormalTexture;
#else
    #if RG_CLEAR_COAT_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
}

float3 GetMaterialClearCoatNormal(uint instanceIndex, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0)
{
    ModelMaterialConstant material = GetMaterialConstant(instanceIndex);
    
    float3 normal = SampleMaterialTexture(material.clearCoatNormalTexture, uv, mipLOD).xyz;
    
    if(IsRGClearCoatNormalTextureEnabled(instanceIndex))
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

void AlphaTest(uint instanceIndex, float2 uv)
{
    float alpha = 1.0;
#if ALBEDO_TEXTURE
    alpha = SampleMaterialTexture(GetMaterialConstant(instanceIndex).albedoTexture, uv, 0).a;
#elif DIFFUSE_TEXTURE
    alpha = SampleMaterialTexture(GetMaterialConstant(instanceIndex).diffuseTexture, uv, 0).a;
#endif
    clip(alpha - GetMaterialConstant(instanceIndex).alphaCutoff);
}

} // namespace model