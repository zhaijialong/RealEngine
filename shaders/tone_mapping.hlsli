#pragma once

#include "common.hlsli"

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

//https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
float3 ACESFitted(float3 color)
{
    //color *= 1.0 / 6.0;

    color = mul(ACESInputMat, color);

	// Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

	// Clamp to [0, 1]
    color = saturate(color);

    return color;
}

float tonemap_curve(float v)
{
    return 1.0 - exp(-v);
}

float3 tonemap_curve(float3 v)
{
    return float3(tonemap_curve(v.r), tonemap_curve(v.g), tonemap_curve(v.b));
}

//https://github.com/EmbarkStudios/kajiya/blob/main/assets/shaders/inc/tonemap.hlsl
float3 neutral_tonemap(float3 col)
{
    float3 ycbcr = RGBToYCbCr(col);

    float bt = tonemap_curve(length(ycbcr.yz) * 2.4);
    float desat = max((bt - 0.7) * 0.8, 0.0);
    desat *= desat;

    float3 desat_col = lerp(col.rgb, ycbcr.xxx, desat);

    float tm_luma = tonemap_curve(ycbcr.x);
    float3 tm0 = col.rgb * max(0.0, tm_luma / max(1e-5, Luminance(col.rgb)));
    float final_mult = 0.97;
    float3 tm1 = tonemap_curve(desat_col);

    col = lerp(tm0, tm1, bt * bt);

    return col * final_mult;
}

//https://github.com/h3r2tic/tony-mc-mapface/blob/main/shader/tony_mc_mapface.hlsl
float3 tony_mc_mapface(float3 stimulus) 
{
    // Apply a non-linear transform that the LUT is encoded with.
    const float3 encoded = stimulus / (stimulus + 1.0);

    // Align the encoded range to texel centers.
    const float LUT_DIMS = 48.0;
    const float3 uv = encoded * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;

    Texture3D tony_mc_mapface_lut = ResourceDescriptorHeap[SceneCB.tonyMcMapfaceTexture];
    SamplerState sampler_linear_clamp = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

    return tony_mc_mapface_lut.SampleLevel(sampler_linear_clamp, uv, 0).xyz;
}


//https://iolite-engine.com/blog_posts/minimal_agx_implementation
// MIT License
//
// Copyright (c) 2024 Missing Deadlines (Benjamin Wrensch)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// All values used to derive this implementation are sourced from Troy's initial AgX implementation/OCIO config file available here:
//   https://github.com/sobotka/AgX

// 0: Default, 1: Golden, 2: Punchy
#define AGX_LOOK 0

// Mean error^2: 3.6705141e-06
float3 agxDefaultContrastApprox(float3 x) 
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
  
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

float3 agx(float3 val) 
{
    const float3x3 agx_mat = float3x3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992,  0.878468636469772,  0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    
    const float min_ev = -12.47393f;
    const float max_ev = 4.026069f;

    // Input transform (inset)
    val = mul(val, agx_mat);
  
    // Log2 space encoding
    val = clamp(log2(val), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);
  
    // Apply sigmoid function approximation
    val = agxDefaultContrastApprox(val);

    return val;
}

float3 agxEotf(float3 val) 
{
    const float3x3 agx_mat_inv = float3x3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    
    // Inverse input transform (outset)
    val = mul(val, agx_mat_inv);
  
    // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
    // NOTE: We're linearizing the output here. Comment/adjust when
    // *not* using a sRGB render target
    val = pow(val, 2.2);

    return val;
}

float3 agxLook(float3 val) 
{
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float luma = dot(val, lw);
  
    // Default
    float3 offset = float3(0.0, 0.0, 0.0);
    float3 slope = float3(1.0, 1.0, 1.0);
    float3 power = float3(1.0, 1.0, 1.0);
    float sat = 1.0;
 
#if AGX_LOOK == 1
    // Golden
    slope = float3(1.0, 0.9, 0.5);
    power = float3(0.8, 0.8, 0.8);
    sat = 0.8;
#elif AGX_LOOK == 2
    // Punchy
    slope = float3(1.0, 1.0, 1.0);
    power = float3(1.35, 1.35, 1.35);
    sat = 1.4;
#endif
  
    // ASC CDL
    val = pow(val * slope + offset, power);
    return luma + sat * (val - luma);
}
