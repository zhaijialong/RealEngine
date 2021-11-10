#pragma once

#include "global_constants.hlsli"

static const float M_PI = 3.141592653f;

float3 DiffuseBRDF(float3 diffuse)
{
    return diffuse; // / M_PI;
}

float3 D_GGX(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
	
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = /*M_PI */denom * denom;
	
    return a2 * rcp(denom);
}

//http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float3 V_SmithGGX(float3 N, float3 V, float3 L, float a)
{
    float a2 = a * a;
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    float G_V = NdotV + sqrt((NdotV - NdotV * a2) * NdotV + a2);
    float G_L = NdotL + sqrt((NdotL - NdotL * a2) * NdotL + a2);
    return rcp(G_V * G_L);
}

float3 F_Schlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H));
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float3 specular, float roughness, out float3 F)
{
    roughness = max(roughness, 0.03);

    float a = roughness * roughness;
    float3 H = normalize(V + L);

    float3 D = D_GGX(N, H, a);
    float3 Vis = V_SmithGGX(N, V, L, a);
    F = F_Schlick(V, H, specular);

    return D * Vis * F;
}

float3 BRDF(float3 L, float3 V, float3 N, float3 diffuse, float3 specular, float roughness)
{
    float3 F;
    float3 specular_brdf = SpecularBRDF(N, V, L, specular, roughness, F);
    float3 diffuse_brdf = DiffuseBRDF(diffuse) * (1.0 - F);    
    
    float NdotL = saturate(dot(N, L));

    return (diffuse_brdf + specular_brdf) * NdotL;
}


half LinearToSrgbChannel(half lin)
{
    if (lin < 0.00313067)
        return lin * 12.92;
    return pow(lin, (1.0 / 2.4)) * 1.055 - 0.055;
}

half3 LinearToSrgb(half3 lin)
{
    return half3(LinearToSrgbChannel(lin.r), LinearToSrgbChannel(lin.g), LinearToSrgbChannel(lin.b));
}

half3 SrgbToLinear(half3 Color)
{
    Color = max(6.10352e-5, Color);
    return Color > 0.04045 ? pow(Color * (1.0 / 1.055) + 0.0521327, 2.4) : Color * (1.0 / 12.92);
}

float LinearToSrgbChannel(float lin)
{
    if (lin < 0.00313067)
        return lin * 12.92;
    return pow(lin, (1.0 / 2.4)) * 1.055 - 0.055;
}

float3 LinearToSrgb(float3 lin)
{
    return float3(LinearToSrgbChannel(lin.r), LinearToSrgbChannel(lin.g), LinearToSrgbChannel(lin.b));
}

float3 SrgbToLinear(float3 Color)
{
    Color = max(6.10352e-5, Color);
    return Color > 0.04045 ? pow(Color * (1.0 / 1.055) + 0.0521327, 2.4) : Color * (1.0 / 12.92);
}


float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}
 
float2 OctNormalEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}
 
float3 OctNormalDecode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

float GetLinearDepth(float depth)
{
    return 1.0f / (depth * CameraCB.linearZParams.x - CameraCB.linearZParams.y);
}

float3 GetWorldPosition(uint2 screenPos, float depth)
{
    float2 screenUV = ((float2) screenPos + 0.5) * float2(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight);
    float4 clipPos = float4((screenUV * 2.0 - 1.0) * float2(1.0, -1.0), depth, 1.0);
    float4 worldPos = mul(CameraCB.mtxViewProjectionInverse, clipPos);
    worldPos.xyz /= worldPos.w;
    
    return worldPos.xyz;
}

float3 GetNdcPos(float4 clipPos)
{
    return clipPos.xyz / clipPos.w;
}

//[-1, 1] -> [0, 1]
float2 GetScreenUV(float2 ndcPos)
{
    return ndcPos * float2(0.5, -0.5) + 0.5;
}

//[-1, 1] -> [0, width/height]
float2 GetScreenPosition(float2 ndcPos)
{
    return GetScreenUV(ndcPos) * float2(SceneCB.viewWidth, SceneCB.viewHeight);
}

//[0, width/height] -> [-1, 1]
float2 GetNdcPosition(float2 screenPos)
{
    float2 screenUV = screenPos * float2(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight);
    return (screenUV * 2.0 - 1.0) * float2(1.0, -1.0);
}

float4 RGBA8UnormToFloat4(uint packed)
{
    float4 unpacked;
    unpacked.x = (float)(packed & 0x000000ff) / 255.0;
    unpacked.y = (float)(((packed >> 8) & 0x000000ff)) / 255.0;
    unpacked.z = (float)(((packed >> 16) & 0x000000ff)) / 255.0;
    unpacked.w = (float)(packed >> 24) / 255.0;
    return unpacked;
}

uint Float4ToRGBA8Unorm(float4 input)
{
    return ((uint(input.x * 255.0 + 0.5)) |
            (uint(input.y * 255.0 + 0.5) << 8) |
            (uint(input.z * 255.0 + 0.5) << 16) |
            (uint(input.w * 255.0 + 0.5) << 24));
}