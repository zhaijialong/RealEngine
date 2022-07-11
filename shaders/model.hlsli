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

float2 ApplyUVTransform(float2 uv, UVTransform transform)
{
    if(transform.bEnable)
    {
        float3x3 translation = float3x3(1, 0, 0, 0, 1, 0, transform.offset.x, transform.offset.y, 1);
        float3x3 rotation = float3x3(cos(transform.rotation), sin(transform.rotation), 0, -sin(transform.rotation), cos(transform.rotation), 0, 0, 0, 1);
        float3x3 scale = float3x3(transform.scale.x, 0, 0, 0, transform.scale.y, 0, 0, 0, 1);

        return mul(mul(mul(float3(uv, 1), scale), rotation), translation).xy;
    }

    return uv;
}
    
float4 SampleMaterialTexture(Texture2D texture, float2 uv, UVTransform transform, float mipLOD)
{
    SamplerState linearSampler = GetMaterialSampler();
    uv = ApplyUVTransform(uv, transform);

#ifdef RAY_TRACING
    return texture.SampleLevel(linearSampler, uv, mipLOD);
#else
    return texture.Sample(linearSampler, uv);
#endif
}

bool IsAlbedoTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
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
#ifdef RAY_TRACING
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
#ifdef RAY_TRACING
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

PbrMetallicRoughness GetMaterialMetallicRoughness(uint instanceIndex, float2 uv, float albedoMipLOD = 0.0, float metallicMipLOD = 0.0)
{
    PbrMetallicRoughness pbrMetallicRoughness;
    pbrMetallicRoughness.albedo = GetMaterialConstant(instanceIndex).albedo.xyz;
    pbrMetallicRoughness.alpha = 1.0;
    pbrMetallicRoughness.metallic = GetMaterialConstant(instanceIndex).metallic;
    pbrMetallicRoughness.roughness = GetMaterialConstant(instanceIndex).roughness;
    pbrMetallicRoughness.ao = 1.0;
    
    if(IsAlbedoTextureEnabled(instanceIndex))
    {
        Texture2D albedoTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).albedoTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).albedoTextureTransform;
        float4 albedo = SampleMaterialTexture(albedoTexture, uv, transform, albedoMipLOD);
        pbrMetallicRoughness.albedo *= albedo.xyz;
        pbrMetallicRoughness.alpha = albedo.w;
    }
    
    if(IsMetallicRoughnessTextureEnabled(instanceIndex))
    {
        Texture2D metallicRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).metallicRoughnessTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).metallicRoughnessTextureTransform;
        float4 metallicRoughness = SampleMaterialTexture(metallicRoughnessTexture, uv, transform, metallicMipLOD);
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
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).specularGlossinessTexture != INVALID_RESOURCE_INDEX;
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
    PbrSpecularGlossiness pbrSpecularGlossiness;
    pbrSpecularGlossiness.diffuse = GetMaterialConstant(instanceIndex).diffuse;
    pbrSpecularGlossiness.specular = GetMaterialConstant(instanceIndex).specular;
    pbrSpecularGlossiness.glossiness = GetMaterialConstant(instanceIndex).glossiness;
    pbrSpecularGlossiness.alpha = 1.0;
    
    if(IsDiffuseTextureEnabled(instanceIndex))
    {
        Texture2D diffuseTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).diffuseTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).diffuseTextureTransform;
        float4 diffuse = SampleMaterialTexture(diffuseTexture, uv, transform, diffuseMipLOD);
        pbrSpecularGlossiness.diffuse *= diffuse.xyz;
        pbrSpecularGlossiness.alpha = diffuse.w;
    }
    
    if(IsSpecularGlossinessTextureEnabled(instanceIndex))
    {
        Texture2D specularGlossinessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).specularGlossinessTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).specularGlossinessTextureTransform;
        float4 specularGlossiness = SampleMaterialTexture(specularGlossinessTexture, uv, transform, specularMipLOD);
        pbrSpecularGlossiness.specular *= specularGlossiness.xyz;
        pbrSpecularGlossiness.glossiness *= specularGlossiness.w;
    }
    
    return pbrSpecularGlossiness;
}

bool IsNormalTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
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
    Texture2D normalTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).normalTexture);
    UVTransform transform = GetMaterialConstant(instanceIndex).normalTextureTransform;
    
    float3 normal = SampleMaterialTexture(normalTexture, uv, transform, mipLOD).xyz;
    
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
    return GetMaterialConstant(instanceIndex).aoTexture != INVALID_RESOURCE_INDEX;
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
        Texture2D aoTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).aoTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).aoTextureTransform;
        ao = SampleMaterialTexture(aoTexture, uv, transform, mipLOD).x;
    }

    return ao;
}

bool IsEmissiveTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).emissiveTexture != INVALID_RESOURCE_INDEX;
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
    float3 emissive = GetMaterialConstant(instanceIndex).emissive;
    if(IsEmissiveTextureEnabled(instanceIndex))
    {
        Texture2D emissiveTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).emissiveTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).emissiveTextureTransform;
        emissive *= SampleMaterialTexture(emissiveTexture, uv, transform, mipLOD).xyz;
    }
    return emissive;
}
    
bool IsAnisotropyTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).anisotropyTexture != INVALID_RESOURCE_INDEX;
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
        Texture2D anisotropyTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).anisotropyTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).anisotropyTextureTransform;
        float2 anisotropy = SampleMaterialTexture(anisotropyTexture, uv, transform, mipLOD).xy * 2.0 - 1.0;
        anisotropicT = T * anisotropy.x + B * anisotropy.y;
    }

    anisotropicT = normalize(anisotropicT - dot(anisotropicT, N) * N); // reproject on normal plane

    return anisotropicT;
}

bool IsSheenColorTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).sheenColorTexture != INVALID_RESOURCE_INDEX;
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
    return GetMaterialConstant(instanceIndex).sheenColorTexture != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).sheenColorTexture == GetMaterialConstant(instanceIndex).sheenRoughnessTexture;
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
    return GetMaterialConstant(instanceIndex).sheenRoughnessTexture != INVALID_RESOURCE_INDEX;
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
    float3 sheenColor = GetMaterialConstant(instanceIndex).sheenColor;
    float sheenRoughness = GetMaterialConstant(instanceIndex).sheenRoughness;
    
    if(IsSheenColorTextureEnabled(instanceIndex))
    {
        Texture2D sheenColorTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).sheenColorTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).sheenColorTextureTransform;
        float4 sheenTexture = SampleMaterialTexture(sheenColorTexture, uv, transform, mipLOD);
        sheenColor *= sheenTexture.xyz;
            
        if (IsSheenColorRoughnessTextureEnabled(instanceIndex))
        {
            sheenRoughness *= sheenTexture.w;
        }
    }
        
    if (IsSheenRoughnessTextureEnabled(instanceIndex) && !IsSheenColorRoughnessTextureEnabled(instanceIndex))
    {
        Texture2D sheenRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).sheenRoughnessTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).sheenRoughnessTextureTransform;
        sheenRoughness *= SampleMaterialTexture(sheenRoughnessTexture, uv, transform, mipLOD).w;
    }

    return float4(sheenColor, sheenRoughness);
}

bool IsClearCoatTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatTexture != INVALID_RESOURCE_INDEX;
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
    return GetMaterialConstant(instanceIndex).clearCoatTexture != INVALID_RESOURCE_INDEX &&
        GetMaterialConstant(instanceIndex).clearCoatTexture == GetMaterialConstant(instanceIndex).clearCoatRoughnessTexture;
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
    return GetMaterialConstant(instanceIndex).clearCoatRoughnessTexture != INVALID_RESOURCE_INDEX;
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
    float clearCoat = GetMaterialConstant(instanceIndex).clearCoat;
    float clearCoatRoughness = GetMaterialConstant(instanceIndex).clearCoatRoughness;

    if(IsClearCoatTextureEnabled(instanceIndex))
    {
        Texture2D clearCoatTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).clearCoatTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).clearCoatTextureTransform;
        float4 clearCoatTex = SampleMaterialTexture(clearCoatTexture, uv, transform, mipLOD);
        clearCoat *= clearCoatTex.r;
            
        if (IsClearCoatRoughnessCombinedTextureEnabled(instanceIndex))
        {
            clearCoatRoughness *= clearCoatTex.g;
        }
    }

    if (IsClearCoatRoughnessTextureEnabled(instanceIndex) && !IsClearCoatRoughnessCombinedTextureEnabled(instanceIndex))
    {
        Texture2D clearCoatRoughnessTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).clearCoatRoughnessTexture);
        UVTransform transform = GetMaterialConstant(instanceIndex).clearCoatRoughnessTextureTransform;
        clearCoatRoughness *= SampleMaterialTexture(clearCoatRoughnessTexture, uv, transform, mipLOD).g;
    }
        
    return float2(clearCoat, clearCoatRoughness);
}

bool IsClearCoatNormalTextureEnabled(uint instanceIndex)
{
#ifdef RAY_TRACING
    return GetMaterialConstant(instanceIndex).clearCoatNormalTexture != INVALID_RESOURCE_INDEX;
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
    Texture2D normalTexture = GetMaterialTexture2D(GetMaterialConstant(instanceIndex).clearCoatNormalTexture);
    UVTransform transform = GetMaterialConstant(instanceIndex).clearCoatNormalTextureTransform;
    
    float3 normal = SampleMaterialTexture(normalTexture, uv, transform, mipLOD).xyz;
    
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