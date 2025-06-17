#pragma once

#include "common.hlsli"

uint GetLightGridSliceIndex(float linearDepth)
{
    // uint sliceIndex = log(linearDepth / GetCameraCB().nearZ) * sliceCount / log(maxSliceDepth / GetCameraCB().nearZ);
    uint sliceIndex = log(linearDepth * SceneCB.lightGridSliceParams.x) * SceneCB.lightGridSliceParams.y;
    
    return clamp(sliceIndex, 0, SceneCB.lightGridSliceCount - 1);
}

uint GetLightGridIndex(uint2 screenPos, float depth)
{
    uint tileCountX = (SceneCB.renderSize.x + SceneCB.lightGridTileSize - 1) / SceneCB.lightGridTileSize;
    uint tileCountY = (SceneCB.renderSize.y + SceneCB.lightGridTileSize - 1) / SceneCB.lightGridTileSize;
    uint tilesPerSlice = tileCountX * tileCountY;
    
    uint2 tilePos = screenPos / SceneCB.lightGridTileSize;
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