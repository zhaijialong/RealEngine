#pragma once

#include "common.hlsli"

// reference : Next Generation Post Processing in Call of Duty: Advanced Warfare [Jimenez14]
//             http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare

float3 BloomDownsample(Texture2D texture, float2 uv, float2 pixelSize)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    return texture.SampleLevel(linearSampler, uv, 0).xyz;;
}

float3 BloomUpsample(Texture2D texture, float2 uv, float2 pixelSize)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearBlackBoarderSampler];
    return texture.SampleLevel(linearSampler, uv, 0).xyz;;
}