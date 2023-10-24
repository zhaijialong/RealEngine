#pragma once

#define MOTION_BLUR_TILE_SIZE 16
#define MOTION_BLUR_SOFT_DEPTH_EXTENT 20.0

#ifndef __cplusplus
#include "../common.hlsli"

// "Next Generation Post Processing in Call of Duty: Advanced Warfare"

float2 DepthCmp(float centerDepth, float sampleDepth, float depthScale)
{
    return saturate(0.5 + float2(depthScale, -depthScale) * (sampleDepth - centerDepth));
}

float2 SpreadCmp(float offsetLen, float2 spreadLen, float pixelToSampleUnitsScale)
{
    return saturate(pixelToSampleUnitsScale * spreadLen - offsetLen + 1.0);
}

float SampleWeight(float centerDepth, float sampleDepth, float offsetLen, float centerSpreadLen, float sampleSpreadLen, float pixelToSampleUnitsScale, float depthScale)
{
    float2 depthCmp = DepthCmp(centerDepth, sampleDepth, depthScale);
    float2 spreadCmp = SpreadCmp(offsetLen, float2(centerSpreadLen, sampleSpreadLen), pixelToSampleUnitsScale);
    return dot(depthCmp, spreadCmp);
}

#endif