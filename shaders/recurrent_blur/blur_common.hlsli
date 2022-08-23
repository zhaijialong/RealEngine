#pragma once

#include "../common.hlsli"

// "Fast Denoising with Self Stabilizing Recurrent Blurs", Dmitry Zhdan, 2020
float GetGeometryWeight(float3 p0, float3 n0, float3 p, float planeDistNorm)
{
    // where planeDistNorm = accumSpeedFactor / ( 1.0 + centerZ );
    // It represents { 1 / "max possible allowed distance between a point and the plane" }
    float3 ray = p - p0;
    float distToPlane = dot(n0, ray);
    float w = saturate(1.0 - abs(distToPlane) * planeDistNorm);
    return w;
}

float GetNormalWeight(float3 n0, float3 n)
{
    return pow(saturate(dot(n0, n)), 128.0);
}

float2 GetSampleUV(float3 worldPos, float3 N, float2 random)
{
    float3x3 tangentBasis = GetTangentBasis(N);
    float3 T = tangentBasis[0];
    float3 B = tangentBasis[1];
    
    worldPos += T * random.x + B * random.y;

    float4 clipPos = mul(CameraCB.mtxViewProjection, float4(worldPos, 1.0));
    float3 ndcPos = GetNdcPosition(clipPos);
    
    return GetScreenUV(ndcPos.xy);
}