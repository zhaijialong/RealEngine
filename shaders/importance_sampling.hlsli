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

float3x3 GetTangentBasis(float3 N)
{
    const float3 B = GetPerpendicularVector(N);
    const float3 T = cross(B, N);

    return float3x3(T, B, N);
}

float3 TangentToWorld(float3 v, float3 N)
{
    float3x3 tangentBasis = GetTangentBasis(N);
    return mul(v, tangentBasis);
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

// http://jcgt.org/published/0007/04/01/paper.pdf
// PDF = G_SmithV * VdotH * D / NdotV = 2.0 * VdotH * D / (NdotV + sqrt(NdotV * (NdotV - NdotV * a2) + a2))
float3 SampleGGXVNDF(float2 randVal, float roughness, float3 N, float3 V)
{
    float a = roughness * roughness;
    float3x3 tangentBasis = GetTangentBasis(N);
    float3 Ve = mul(tangentBasis, V); //world to tangent

    // Section 3.2: transforming the view direction to the hemisphere configuration
    float3 Vh = normalize(float3(a * Ve.x, a * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(randVal.x);
    float phi = 2.0 * M_PI * randVal.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    float3 Ne = normalize(float3(a * Nh.x, a * Nh.y, max(0.0, Nh.z)));

    return mul(Ne, tangentBasis); //tangent to world
}