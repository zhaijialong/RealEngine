#pragma once

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
    float3 _padding;

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