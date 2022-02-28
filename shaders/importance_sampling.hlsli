#pragma once

#include "common.hlsli"

float3 TangentToWorld(float3 v, float3 N)
{
    const float3 B = GetPerpendicularVector(N);
    const float3 T = cross(B, N);
    return T * v.x + B * v.y + N * v.z;
}

//PDF = cos / PI = NdotL / PI
float3 SampleCosHemisphere(float2 randVal, float3 N)
{
    float r = sqrt(randVal.x);
    float phi = 2.0 * M_PI * randVal.y;

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    float3 v = float3(r * cos(phi), r * sin(phi), sqrt(1.0 - randVal.x));

    return TangentToWorld(v, N);
}