#pragma once

#include "../common.hlsli"

cbuffer Rootconstants : register(b0)
{
    float c_maxRoughness;
    float c_temporalStability;
};

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
    return roughness <= c_maxRoughness;
}

bool FFX_DNSR_Reflections_IsMirrorReflection(float roughness)
{
    return roughness < 0.03;
}

float3 FFX_DNSR_Reflections_ScreenSpaceToViewSpace(float3 screen_uv_coord)
{
    screen_uv_coord.y = (1 - screen_uv_coord.y);
    screen_uv_coord.xy = 2 * screen_uv_coord.xy - 1;
    float4 view_pos = mul(CameraCB.mtxProjectionInverse, float4(screen_uv_coord, 1));
    view_pos.xyz /= view_pos.w;
    return view_pos.xyz;
}

float3 FFX_DNSR_Reflections_ViewSpaceToWorldSpace(float4 view_space_coord)
{
    return mul(CameraCB.mtxViewInverse, view_space_coord).xyz;
}

float3 FFX_DNSR_Reflections_WorldSpaceToScreenSpacePrevious(float3 world_space_pos)
{
    float4 projected = mul(CameraCB.mtxPrevViewProjection, float4(world_space_pos, 1));
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    projected.y = (1 - projected.y);
    return projected.xyz;
}

float FFX_DNSR_Reflections_GetLinearDepth(float2 uv, float depth)
{
    return GetLinearDepth(depth);
}