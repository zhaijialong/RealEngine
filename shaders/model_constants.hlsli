#pragma once

#include "texture.hlsli"

struct ModelMaterialConstant
{    
    float3 albedo;
    float alphaCutoff;

    float metallic;
    float3 specular;
    
    float roughness;
    float3 diffuse;
    
    float glossiness;
    float3 emissive;
    
    uint shadingModel;
    float anisotropy;
    float sheenRoughness;
    float clearCoatRoughness;
    
    float3 sheenColor;
    float clearCoat;

    MaterialTextureInfo albedoTexture;
    MaterialTextureInfo metallicRoughnessTexture;
    MaterialTextureInfo normalTexture;
    MaterialTextureInfo emissiveTexture;
    MaterialTextureInfo aoTexture;
    MaterialTextureInfo diffuseTexture;
    MaterialTextureInfo specularGlossinessTexture;
    MaterialTextureInfo anisotropyTexture;
    MaterialTextureInfo sheenColorTexture;
    MaterialTextureInfo sheenRoughnessTexture;
    MaterialTextureInfo clearCoatTexture;
    MaterialTextureInfo clearCoatRoughnessTexture;
    MaterialTextureInfo clearCoatNormalTexture;

    uint bPbrMetallicRoughness;
    uint bPbrSpecularGlossiness;
    uint bRGNormalTexture;
    uint bDoubleSided;
    
    uint bRGClearCoatNormalTexture;
    uint3 _padding;
};

#ifndef __cplusplus
cbuffer RootConstants : register(b0)
{
    uint c_InstanceIndex;
    uint c_PrevAnimationPosAddress; //only used for velocity pass
};
#endif