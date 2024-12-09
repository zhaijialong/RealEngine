#pragma once

#include "common.hlsli"
#include "brdf.hlsli"
#include "gpu_scene.hlsli"

// https://lisyarus.github.io/blog/posts/point-light-attenuation.html
float PointLightAttenuation(float distance, float radius, float falloff)
{
    float s = distance / radius;
    if(s >= 1.0)
    {
        return 0.0;
    }
    
    float s2 = square(s);
    return square(1.0 - s2) / (1.0 + falloff * s2);
}

float3 CalculatePointLight(LocalLightData light, float3 worldPosition, ShadingModel shadingModel, float3 V, float3 N, float3 diffuse, float3 specular, float roughness, float4 customData)
{
    float3 ToLight = light.position - worldPosition;
    float distance = length(ToLight);
    float3 L = ToLight / distance;
    float attenuation = PointLightAttenuation(distance, light.radius, light.falloff);
    return EvaluateBRDF(shadingModel, L, V, N, diffuse, specular, roughness, customData, light.color) * attenuation;
}

float3 CalculateSpotLight(LocalLightData light, float3 worldPosition, ShadingModel shadingModel, float3 V, float3 N, float3 diffuse, float3 specular, float roughness, float4 customData)
{
    //todo
    return 0.0;
}

float3 CalculateRectLight(LocalLightData light, float3 worldPosition, ShadingModel shadingModel, float3 V, float3 N, float3 diffuse, float3 specular, float roughness, float4 customData)
{
    //todo;
    return 0.0;
}

float3 CalculateLocalLight(LocalLightData light, float3 worldPosition, ShadingModel shadingModel, float3 V, float3 N, float3 diffuse, float3 specular, float roughness, float4 customData)
{
    switch (light.GetLocalLightType())
    {
        case LocalLightType::Point:
            return CalculatePointLight(light, worldPosition, shadingModel, V, N, diffuse, specular, roughness, customData);
        case LocalLightType::Spot:
            return CalculateSpotLight(light, worldPosition, shadingModel, V, N, diffuse, specular, roughness, customData);
        case LocalLightType::Rect:
            return CalculateRectLight(light, worldPosition, shadingModel, V, N, diffuse, specular, roughness, customData);
        default:
            break;
    }
    
    return 0.0;
}