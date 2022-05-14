#pragma once

#include "../common.hlsli"

uint FFX_DNSR_Reflections_BitfieldExtract(uint src, uint off, uint bits)
{
    uint mask = (1u << bits) - 1;
    return (src >> off) & mask;
}

uint FFX_DNSR_Reflections_BitfieldInsert(uint src, uint ins, uint bits)
{
    uint mask = (1u << bits) - 1;
    return (ins & mask) | (src & (~mask));
}

//  LANE TO 8x8 MAPPING
//  ===================
//  00 01 08 09 10 11 18 19
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f
//  20 21 28 29 30 31 38 39
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f
uint2 FFX_DNSR_Reflections_RemapLane8x8(uint lane)
{
    return uint2(FFX_DNSR_Reflections_BitfieldInsert(FFX_DNSR_Reflections_BitfieldExtract(lane, 2u, 3u), lane, 1u),
                 FFX_DNSR_Reflections_BitfieldInsert(FFX_DNSR_Reflections_BitfieldExtract(lane, 3u, 3u), FFX_DNSR_Reflections_BitfieldExtract(lane, 1u, 2u), 2u));
}

uint PackRayCoords(uint2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal)
{
    uint ray_x_15bit = ray_coord.x & 0b111111111111111;
    uint ray_y_14bit = ray_coord.y & 0b11111111111111;
    uint copy_horizontal_1bit = copy_horizontal ? 1 : 0;
    uint copy_vertical_1bit = copy_vertical ? 1 : 0;
    uint copy_diagonal_1bit = copy_diagonal ? 1 : 0;

    uint packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

void UnpackRayCoords(uint packed, out uint2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal)
{
    ray_coord.x = (packed >> 0) & 0b111111111111111;
    ray_coord.y = (packed >> 15) & 0b11111111111111;
    copy_horizontal = (packed >> 29) & 0b1;
    copy_vertical = (packed >> 30) & 0b1;
    copy_diagonal = (packed >> 31) & 0b1;
}

//=== FFX_DNSR_Reflections_ override functions ===

bool FFX_DNSR_Reflections_IsGlossyReflection(float roughness)
{
    const float roughness_threshold = 1.0; //todo : don't shoot a ray on very rough surfaces
    return roughness <= roughness_threshold;
}

bool FFX_DNSR_Reflections_IsMirrorReflection(float roughness)
{
    return roughness < 0.03;
}