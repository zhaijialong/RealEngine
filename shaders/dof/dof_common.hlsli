#pragma once

#include "../common.hlsli"

// references :
// "Circular Separable Convolution Depth of Field", Kleber Garcia, 2018
// https://www.shadertoy.com/view/Xd2BWc

static const float MAX_COC_SIZE = 32.0;

static const int KERNEL_RADIUS = 8;
static const int KERNEL_COUNT = 17;

// 2-Component

static const float4 Kernel0BracketsRealXY_ImZW_2 = float4(-0.038708, 0.943062, -0.025574, 0.660892);
static const float2 Kernel0Weights_RealX_ImY_2 = float2(0.411259, -0.548794);
static const float4 Kernel0_RealX_ImY_RealZ_ImW_2[] =
    {
        float4( /*XY: Non Bracketed*/0.014096, -0.022658, /*Bracketed WZ:*/0.055991, 0.004413),
        float4( /*XY: Non Bracketed*/-0.020612, -0.025574, /*Bracketed WZ:*/0.019188, 0.000000),
        float4( /*XY: Non Bracketed*/-0.038708, 0.006957, /*Bracketed WZ:*/0.000000, 0.049223),
        float4( /*XY: Non Bracketed*/-0.021449, 0.040468, /*Bracketed WZ:*/0.018301, 0.099929),
        float4( /*XY: Non Bracketed*/0.013015, 0.050223, /*Bracketed WZ:*/0.054845, 0.114689),
        float4( /*XY: Non Bracketed*/0.042178, 0.038585, /*Bracketed WZ:*/0.085769, 0.097080),
        float4( /*XY: Non Bracketed*/0.057972, 0.019812, /*Bracketed WZ:*/0.102517, 0.068674),
        float4( /*XY: Non Bracketed*/0.063647, 0.005252, /*Bracketed WZ:*/0.108535, 0.046643),
        float4( /*XY: Non Bracketed*/0.064754, 0.000000, /*Bracketed WZ:*/0.109709, 0.038697),
        float4( /*XY: Non Bracketed*/0.063647, 0.005252, /*Bracketed WZ:*/0.108535, 0.046643),
        float4( /*XY: Non Bracketed*/0.057972, 0.019812, /*Bracketed WZ:*/0.102517, 0.068674),
        float4( /*XY: Non Bracketed*/0.042178, 0.038585, /*Bracketed WZ:*/0.085769, 0.097080),
        float4( /*XY: Non Bracketed*/0.013015, 0.050223, /*Bracketed WZ:*/0.054845, 0.114689),
        float4( /*XY: Non Bracketed*/-0.021449, 0.040468, /*Bracketed WZ:*/0.018301, 0.099929),
        float4( /*XY: Non Bracketed*/-0.038708, 0.006957, /*Bracketed WZ:*/0.000000, 0.049223),
        float4( /*XY: Non Bracketed*/-0.020612, -0.025574, /*Bracketed WZ:*/0.019188, 0.000000),
        float4( /*XY: Non Bracketed*/0.014096, -0.022658, /*Bracketed WZ:*/0.055991, 0.004413)
    };

static const float4 Kernel1BracketsRealXY_ImZW_2 = float4(0.000115, 0.559524, 0.000000, 0.178226);
static const float2 Kernel1Weights_RealX_ImY_2 = float2(0.513282, 4.561110);
static const float4 Kernel1_RealX_ImY_RealZ_ImW_2[] =
    {
        float4( /*XY: Non Bracketed*/0.000115, 0.009116, /*Bracketed WZ:*/0.000000, 0.051147),
        float4( /*XY: Non Bracketed*/0.005324, 0.013416, /*Bracketed WZ:*/0.009311, 0.075276),
        float4( /*XY: Non Bracketed*/0.013753, 0.016519, /*Bracketed WZ:*/0.024376, 0.092685),
        float4( /*XY: Non Bracketed*/0.024700, 0.017215, /*Bracketed WZ:*/0.043940, 0.096591),
        float4( /*XY: Non Bracketed*/0.036693, 0.015064, /*Bracketed WZ:*/0.065375, 0.084521),
        float4( /*XY: Non Bracketed*/0.047976, 0.010684, /*Bracketed WZ:*/0.085539, 0.059948),
        float4( /*XY: Non Bracketed*/0.057015, 0.005570, /*Bracketed WZ:*/0.101695, 0.031254),
        float4( /*XY: Non Bracketed*/0.062782, 0.001529, /*Bracketed WZ:*/0.112002, 0.008578),
        float4( /*XY: Non Bracketed*/0.064754, 0.000000, /*Bracketed WZ:*/0.115526, 0.000000),
        float4( /*XY: Non Bracketed*/0.062782, 0.001529, /*Bracketed WZ:*/0.112002, 0.008578),
        float4( /*XY: Non Bracketed*/0.057015, 0.005570, /*Bracketed WZ:*/0.101695, 0.031254),
        float4( /*XY: Non Bracketed*/0.047976, 0.010684, /*Bracketed WZ:*/0.085539, 0.059948),
        float4( /*XY: Non Bracketed*/0.036693, 0.015064, /*Bracketed WZ:*/0.065375, 0.084521),
        float4( /*XY: Non Bracketed*/0.024700, 0.017215, /*Bracketed WZ:*/0.043940, 0.096591),
        float4( /*XY: Non Bracketed*/0.013753, 0.016519, /*Bracketed WZ:*/0.024376, 0.092685),
        float4( /*XY: Non Bracketed*/0.005324, 0.013416, /*Bracketed WZ:*/0.009311, 0.075276),
        float4( /*XY: Non Bracketed*/0.000115, 0.009116, /*Bracketed WZ:*/0.000000, 0.051147)
    };

// 1-Component

static const float4 Kernel0BracketsRealXY_ImZW_1 = float4(-0.001442, 0.672786, 0.000000, 0.311371);
static const float2 Kernel0Weights_RealX_ImY_1 = float2(0.767583, 1.862321);
static const float4 Kernel0_RealX_ImY_RealZ_ImW_1[] =
    {
        float4( /*XY: Non Bracketed*/-0.001442, 0.026656, /*Bracketed WZ:*/0.000000, 0.085609),
        float4( /*XY: Non Bracketed*/0.010488, 0.030945, /*Bracketed WZ:*/0.017733, 0.099384),
        float4( /*XY: Non Bracketed*/0.023771, 0.030830, /*Bracketed WZ:*/0.037475, 0.099012),
        float4( /*XY: Non Bracketed*/0.036356, 0.026770, /*Bracketed WZ:*/0.056181, 0.085976),
        float4( /*XY: Non Bracketed*/0.046822, 0.020140, /*Bracketed WZ:*/0.071737, 0.064680),
        float4( /*XY: Non Bracketed*/0.054555, 0.012687, /*Bracketed WZ:*/0.083231, 0.040745),
        float4( /*XY: Non Bracketed*/0.059606, 0.006074, /*Bracketed WZ:*/0.090738, 0.019507),
        float4( /*XY: Non Bracketed*/0.062366, 0.001584, /*Bracketed WZ:*/0.094841, 0.005086),
        float4( /*XY: Non Bracketed*/0.063232, 0.000000, /*Bracketed WZ:*/0.096128, 0.000000),
        float4( /*XY: Non Bracketed*/0.062366, 0.001584, /*Bracketed WZ:*/0.094841, 0.005086),
        float4( /*XY: Non Bracketed*/0.059606, 0.006074, /*Bracketed WZ:*/0.090738, 0.019507),
        float4( /*XY: Non Bracketed*/0.054555, 0.012687, /*Bracketed WZ:*/0.083231, 0.040745),
        float4( /*XY: Non Bracketed*/0.046822, 0.020140, /*Bracketed WZ:*/0.071737, 0.064680),
        float4( /*XY: Non Bracketed*/0.036356, 0.026770, /*Bracketed WZ:*/0.056181, 0.085976),
        float4( /*XY: Non Bracketed*/0.023771, 0.030830, /*Bracketed WZ:*/0.037475, 0.099012),
        float4( /*XY: Non Bracketed*/0.010488, 0.030945, /*Bracketed WZ:*/0.017733, 0.099384),
        float4( /*XY: Non Bracketed*/-0.001442, 0.026656, /*Bracketed WZ:*/0.000000, 0.085609)
    };

float2 MulComplex(float2 p, float2 q)
{
    return float2(p.x * q.x - p.y * q.y, p.x * q.y + p.y * q.x);
}

float ComputeCoc(float depth, float apertureWidth, float focalLength, float focusDistance, float sensorWidth)
{
    float linearDepth = GetLinearDepth(max(depth, 1e-7));
    
    // https://en.wikipedia.org/wiki/Circle_of_confusion#Determining_a_circle_of_confusion_diameter_from_the_object_field
    float coc = apertureWidth * focalLength * (linearDepth - focusDistance) / (linearDepth * (focusDistance - focalLength));
    coc = coc * SceneCB.renderSize.x * 0.5 / sensorWidth;
    
    return clamp(coc / MAX_COC_SIZE, -1.0, 1.0);
}