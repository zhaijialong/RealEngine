#pragma once

struct UVTransform
{
    float2 offset;
    float2 scale;
    
    float rotation;
    uint bEnable;
    float2 _padding;
};

struct ModelMaterialConstant
{
    uint albedoTexture;
    uint metallicRoughnessTexture;
    uint normalTexture;
    uint emissiveTexture;
    
    uint aoTexture;
    float3 albedo;
    
    float metallic;
    float roughness;
    float glossiness;
    float alphaCutoff;
    
    uint diffuseTexture;
    float3 diffuse;
    
    uint specularGlossinessTexture;
    float3 specular;
    
    float3 emissive;
    uint shadingModel;
    
    float anisotropy;
    uint anisotropyTexture;
    uint sheenRoughnessTexture;
    float sheenRoughness;
    
    uint sheenColorTexture;
    float3 sheenColor;
    
    uint clearCoatTexture;
    float clearCoat;
    uint clearCoatRoughnessTexture;
    float clearCoatRoughness;
    
    uint clearCoatNormalTexture;
    uint bRGClearCoatNormalTexture;
    uint3 _padding;

    UVTransform albedoTextureTransform;
    UVTransform metallicRoughnessTextureTransform;
    UVTransform normalTextureTransform;
    UVTransform emissiveTextureTransform;
    UVTransform aoTextureTransform;
    UVTransform diffuseTextureTransform;
    UVTransform specularGlossinessTextureTransform;
    UVTransform anisotropyTextureTransform;
    UVTransform sheenColorTextureTransform;
    UVTransform sheenRoughnessTextureTransform;
    UVTransform clearCoatTextureTransform;
    UVTransform clearCoatRoughnessTextureTransform;
    UVTransform clearCoatNormalTextureTransform;

    uint bPbrMetallicRoughness;
    uint bPbrSpecularGlossiness;
    uint bRGNormalTexture;
    uint bDoubleSided;
};

#ifndef __cplusplus
cbuffer RootConstants : register(b0)
{
    uint c_InstanceIndex;
    uint c_PrevAnimationPosAddress; //only used for velocity pass
};
#endif