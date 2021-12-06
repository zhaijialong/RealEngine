#pragma once

struct ModelConstant
{
    uint posBuffer;
    uint uvBuffer;
    uint normalBuffer;
    uint tangentBuffer;    
    
    uint prevPosBuffer;
    uint2 _padding;
    float scale;
    
    float3 center;
    float radius;
    
    float4x4 mtxWorld;
    float4x4 mtxNormal;
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
    
    float3 emissive;
    float metallic;

    float roughness;
    float alphaCutoff;
};

#ifndef __cplusplus
ConstantBuffer<ModelConstant> ModelCB : register(b1);
ConstantBuffer<MaterialConstant> MaterialCB : register(b2);
#endif