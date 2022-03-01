#pragma once

#include "common.hlsli"

float3 TangentToWorld(float3 v, float3 N)
{
    const float3 B = GetPerpendicularVector(N);
    const float3 T = cross(B, N);
    return T * v.x + B * v.y + N * v.z;
}

// PDF = cos / PI = NdotL / PI
float3 SampleCosHemisphere(float2 randVal, float3 N)
{
    float r = sqrt(randVal.x);
    float phi = 2.0 * M_PI * randVal.y;

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    float3 v = float3(r * cos(phi), r * sin(phi), sqrt(1.0 - randVal.x));

    return TangentToWorld(v, N);
}

// PDF = D * NdotH / (4 * HdotV)
float3 SampleGGX(float2 randVal, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float cosThetaH = sqrt(max(0.0f, (1.0 - randVal.x) / ((a2 - 1.0) * randVal.x + 1)));
    float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
    float phiH = randVal.y * M_PI * 2.0f;

    float3 v = float3(sinThetaH * cos(phiH), sinThetaH * sin(phiH), cosThetaH);

    return TangentToWorld(v, N);
}