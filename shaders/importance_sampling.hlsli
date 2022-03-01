#pragma once

#include "common.hlsli"

// "Efficient Construction of Perpendicular Vectors Without Branching"
float3 GetPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

float3 TangentToWorld(float3 v, float3 N)
{
    const float3 B = GetPerpendicularVector(N);
    const float3 T = cross(B, N);
    return T * v.x + B * v.y + N * v.z;
}

float2 SampleDiskUniform(float2 randVal)
{
    float r = sqrt(randVal.x);
    float phi = 2.0 * M_PI * randVal.y;

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    return r * float2(cosPhi, sinPhi);
}

float3 SampleConeUniform(float2 randVal, float radius, float3 N)
{
    float cos_theta = cos(radius);
    float r0 = cos_theta + randVal.x * (1.0f - cos_theta);
    float r = sqrt(max(0.0, 1.0 - r0 * r0));
    float phi = 2.0 * M_PI * randVal.y;
    
    float3 v = float3(r * cos(phi), r * sin(phi), r0);

    return TangentToWorld(v, N);
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
