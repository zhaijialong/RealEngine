#pragma once

#include "../common.hlsli"

// generated with https://github.com/bartwronski/PoissonSamplingGenerator
static const uint SAMPLE_NUM = 8;
static const float2 POISSON_SAMPLES[SAMPLE_NUM] =
{
    float2(-0.5896551877516465f, 0.3042686696860047f),
    float2(0.3977245633422762f, -0.8996213214091384f),
    float2(0.6928542993967035f, 0.4045607151275282f),
    float2(-0.5838642467498989f, -0.7331419984727449f),
    float2(-0.15857706879313935f, 0.9805939290016509f),
    float2(0.4809628104153716f, -0.23742957535249992f),
    float2(-0.03046610330580737f, 0.4220034509328376f),
    float2(-0.2770630908672257f, -0.16925817178494032f),
};

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
