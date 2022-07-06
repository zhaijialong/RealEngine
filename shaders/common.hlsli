#pragma once

#include "global_constants.hlsli"

static const float M_PI = 3.141592653f;

static const uint INVALID_RESOURCE_INDEX = 0xFFFFFFFF;
static const uint INVALID_ADDRESS = 0xFFFFFFFF;

float max3(float3 v)
{
    return max(max(v.x, v.y), v.z);
}

template<typename T>
T LinearToSrgbChannel(T color)
{
    return color < 0.00313067 ? color * 12.92 : pow(color, (1.0 / 2.4)) * 1.055 - 0.055;
}

template<typename T>
T SrgbToLinearChannel(T color)
{
    return color > 0.04045 ? pow(color * (1.0 / 1.055) + 0.0521327, 2.4) : color * (1.0 / 12.92);
}

template<typename T>
T LinearToSrgb(T color)
{
    return T(LinearToSrgbChannel(color.r), LinearToSrgbChannel(color.g), LinearToSrgbChannel(color.b));
}

template<typename T>
T SrgbToLinear(T color)
{
    return T(SrgbToLinearChannel(color.r), SrgbToLinearChannel(color.g), SrgbToLinearChannel(color.b));
}

float3 RGBToYCbCr(float3 color)
{
    const float3x3 m = float3x3(0.2126, 0.7152, 0.0722, -0.1146, -0.3854, 0.5, 0.5, -0.4542, -0.0458);
    return mul(m, color);
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}

//pack float2[0,1] in rgb8unorm, each float is 12 bits
float3 EncodeRGB8Unorm(float2 v)
{
    uint2 int12 = (uint2)round(v * 4095);
    uint3 int8 = uint3(int12.x & 0xFF, int12.y & 0xFF, ((int12.x >> 4) & 0xF0) | ((int12.y >> 8) & 0xF));
    return int8 / 255.0;
}

float2 DecodeRGB8Unorm(float3 v)
{
    uint3 int8 = (uint3)round(v * 255.0);
    uint2 int12 = uint2(int8.x | ((int8.z & 0xF0) << 4), int8.y | ((int8.z & 0xF) << 8));
    return int12 / 4095.0;
}

float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    //n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * lerp(-1.0, 1.0, n.xy >= 0.0);

    return n.xy;
}

// https://twitter.com/Stubbesaurus/status/937994790553227264
float3 OctDecode(float2 f)
{
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    //n.xy += n.xy >= 0.0 ? -t : t;
    n.xy += lerp(t, -t, n.xy >= 0.0);
    return normalize(n);
}
 
float3 EncodeNormal(float3 n)
{
    float2 v = OctEncode(n) * 0.5 + 0.5;
    return EncodeRGB8Unorm(v);
}

float3 DecodeNormal(float3 f)
{
    float2 v = DecodeRGB8Unorm(f) * 2.0 - 1.0;
    return OctDecode(v);
}

float2 EncodeNormalLQ(float3 n)
{
    return OctEncode(n) * 0.5 + 0.5;
}

float3 DecodeNormalLQ(float2 f)
{
    return OctDecode(f * 2.0 - 1.0);
}

float4 EncodeAnisotropy(float3 T, float anisotropy)
{
    return float4(EncodeNormal(T), anisotropy * 0.5 + 0.5);
}

void DecodeAnisotropy(float4 data, out float3 T, out float anisotropy)
{
    T = DecodeNormal(data.xyz);
    anisotropy = data.w * 2.0 - 1.0;
}

float GetLinearDepth(float ndcDepth)
{
    float C1 = CameraCB.linearZParams.x;
    float C2 = CameraCB.linearZParams.y;
    
    return 1.0f / (ndcDepth * C1 - C2);
}

float GetNdcDepth(float linearDepth)
{
    float A = CameraCB.mtxProjection._33;
    float B = CameraCB.mtxProjection._34;
    
    return A + B / linearDepth;
}

float3 GetNdcPosition(float4 clipPos)
{
    return clipPos.xyz / max(clipPos.w, 0.0000001);
}

//[0, width/height] -> [-1, 1]
float2 GetNdcPosition(float2 screenPos, float2 rcpBufferSize)
{
    float2 screenUV = screenPos * rcpBufferSize;
    return (screenUV * 2.0 - 1.0) * float2(1.0, -1.0);
}

//[0, width/height] -> [0, 1]
float2 GetScreenUV(uint2 screenPos, float2 rcpBufferSize)
{
    return ((float2)screenPos + 0.5) * rcpBufferSize;
}

//[-1, 1] -> [0, 1]
float2 GetScreenUV(float2 ndcPos)
{
    return ndcPos * float2(0.5, -0.5) + 0.5;
}

//[-1, 1] -> [0, width/height]
float2 GetScreenPosition(float2 ndcPos, uint2 bufferSize)
{
    return GetScreenUV(ndcPos) * bufferSize;
}

float3 GetWorldPosition(float2 screenUV, float depth)
{
    float4 clipPos = float4((screenUV * 2.0 - 1.0) * float2(1.0, -1.0), depth, 1.0);
    float4 worldPos = mul(CameraCB.mtxViewProjectionInverse, clipPos);
    worldPos.xyz /= worldPos.w;
    
    return worldPos.xyz;
}

float3 GetWorldPosition(uint2 screenPos, float depth)
{
    return GetWorldPosition(GetScreenUV(screenPos, SceneCB.rcpRenderSize), depth);
}

float4 RGBA8UnormToFloat4(uint packed)
{
    //uint16_t4 unpacked = unpack_u8u16((uint8_t4_packed)packed);
    uint32_t4 unpacked = unpack_u8u32((uint8_t4_packed)packed);

    return unpacked / 255.0f;
}

uint Float4ToRGBA8Unorm(float4 input)
{
    //uint16_t4 unpacked = uint16_t4(input * 255.0 + 0.5);
    uint32_t4 unpacked = uint32_t4(input * 255.0 + 0.5);
    
    return (uint)pack_u8(unpacked);
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool ProjectSphere(float3 center, float radius, float znear, float P00, float P11, out float4 aabb)
{
    if (center.z < radius + znear)
    {
        return false;
    }

    float2 cx = -center.xz;
    float2 vx = float2(sqrt(dot(cx, cx) - radius * radius), radius);
    float2 minx = mul(cx, float2x2(float2(vx.x, vx.y), float2(-vx.y, vx.x)));
    float2 maxx = mul(cx, float2x2(float2(vx.x, -vx.y), float2(vx.y, vx.x)));

    float2 cy = -center.yz;
    float2 vy = float2(sqrt(dot(cy, cy) - radius * radius), radius);
    float2 miny = mul(cy, float2x2(float2(vy.x, vy.y), float2(-vy.y, vy.x)));
    float2 maxy = mul(cy, float2x2(float2(vy.x, -vy.y), float2(vy.y, vy.x)));

    aabb = float4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
    aabb = aabb.xwzy * float4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f; // clip space -> uv space

    return true;
}

bool OcclusionCull(Texture2D<float> hzbTexture, uint2 hzbSize, float3 center, float radius)
{
    center = mul(CameraCB.mtxView, float4(center, 1.0)).xyz;
    
    bool visible = true;

    float4 aabb;
    if (ProjectSphere(center, radius, CameraCB.nearZ, CameraCB.mtxProjection[0][0], CameraCB.mtxProjection[1][1], aabb))
    {
        SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.minReductionSampler];
        
        float width = (aabb.z - aabb.x) * hzbSize.x;
        float height = (aabb.w - aabb.y) * hzbSize.y;
        float level = ceil(log2(max(width, height)));
        
        float depth = hzbTexture.SampleLevel(minReductionSampler, (aabb.xy + aabb.zw) * 0.5, level).x;
        float depthSphere = GetNdcDepth(center.z - radius);

        visible = depthSphere > depth; //reversed depth
    }
    
    return visible;
}

float3 SpecularIBL(float3 radiance, float3 N, float3 V, float roughness, float3 specular)
{
    Texture2D brdfTexture = ResourceDescriptorHeap[SceneCB.preintegratedGFTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    
    float NdotV = saturate(dot(N, V));
    float2 preintegratedGF = brdfTexture.Sample(linearSampler, float2(NdotV, roughness)).xy;
    
    return radiance * (specular * preintegratedGF.x + preintegratedGF.y);
}

float3 SpecularIBL(float3 N, float3 V, float roughness, float3 specular)
{
    TextureCube envTexture = ResourceDescriptorHeap[SceneCB.skySpecularIBLTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    
    const float MAX_LOD = 7.0; //8 mips

    float3 R = reflect(-V, N);
    float3 prefilteredColor = envTexture.SampleLevel(linearSampler, R, roughness * MAX_LOD).rgb;
    
    return SpecularIBL(prefilteredColor, N, V, roughness, specular);
}

float3 DiffuseIBL(float3 N)
{
    TextureCube envTexture = ResourceDescriptorHeap[SceneCB.skyDiffuseIBLTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];

    return envTexture.SampleLevel(linearSampler, N, 0.0).xyz;
}
