#pragma once

#define MOTION_BLUR_TILE_SIZE 16
#define MOTION_BLUR_SOFT_DEPTH_EXTENT 20.0

#ifndef __cplusplus
#include "../common.hlsli"

// velocity : [-1, 1], ndc space
// output : in polar coordinates, texture space
float2 EncodeVelocity(float2 velocity)
{
    float2 velocityInPixels = velocity * float2(0.5, -0.5) * SceneCB.displaySize;
    float velocityLength = length(velocityInPixels);
    if (velocityLength < 0.01)
    {
        return 0.0;
    }
    
    float angle = atan2(velocityInPixels.y, velocityInPixels.x);
    angle = angle * (0.5 / M_PI) + 0.5; // [0, 1]
    
    return float2(velocityLength, angle);
}

float2 DecodeVelocity(float2 velocity)
{
    float angle = velocity.y * (2.0 * M_PI) - M_PI; //[-pi, pi]
    
    float s, c;
    sincos(angle, s, c);
    
    float2 velocityInPixels = velocity.x * float2(c, s);
    return velocityInPixels * float2(2.0, -2.0) * SceneCB.rcpDisplaySize;
}

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