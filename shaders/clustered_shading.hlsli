#pragma once

#include "common.hlsli"

// todo : move these to cb
static const uint tileSize = 48;
static const uint sliceCount = 16;
static const float maxSliceDepth = 500.0f;

uint GetLightGridSliceIndex(float linearDepth)
{
    uint sliceIndex = log(abs(linearDepth) / GetCameraCB().nearZ) * sliceCount / log(maxSliceDepth / GetCameraCB().nearZ);
    
    return clamp(sliceIndex, 0, sliceCount - 1);
}

uint GetLightGridIndex(uint2 screenPos, float depth)
{
    uint tileCountX = (SceneCB.renderSize.x + tileSize - 1) / tileSize;
    uint tileCountY = (SceneCB.renderSize.y + tileSize - 1) / tileSize;
    uint tilesPerSlice = tileCountX * tileCountY;
    
    uint2 tilePos = screenPos / tileSize;
    uint sliceIndex = GetLightGridSliceIndex(GetLinearDepth(depth));
    return tilePos.x + tilePos.y * tileCountX + sliceIndex * tilesPerSlice;
}

// x : offset, y : count
uint2 GetLightGridData(uint lightGridIndex)
{
    return LoadSceneConstantBuffer<uint2>(SceneCB.lightGridsAddress + lightGridIndex * sizeof(uint2));
}

uint GetLightIndex(uint lightIndicesOffset)
{
    return LoadSceneConstantBuffer<uint>(SceneCB.lightIndicesAddress + lightIndicesOffset * sizeof(uint));
}