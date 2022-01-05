#pragma once

struct ModelConstant
{
    uint meshletCount;
    uint meshletBufferAddress;
    uint meshletVerticesBufferAddress;
    uint meshletIndicesBufferAddress;

    uint posBufferAddress;
    uint uvBufferAddress;
    uint normalBufferAddress;
    uint tangentBufferAddress;
    
    uint prevPosBuffer;
    uint objectID;
    uint _padding;
    float scale;
    
    float3 center;
    float radius;
    
    float4x4 mtxWorld;
    float4x4 mtxWorldInverseTranspose;
    float4x4 mtxPrevWorld;
};

struct MaterialConstant
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
    float _padding;
};

struct ModelInstanceConstant
{
    ModelConstant modelCB;
    MaterialConstant materialCB;
};

#ifndef __cplusplus
cbuffer RootConstants : register(b0)
{
    uint c_SceneConstantAddress;
};

ConstantBuffer<ModelConstant> ModelCB : register(b1);
ConstantBuffer<MaterialConstant> MaterialCB : register(b2);
#endif