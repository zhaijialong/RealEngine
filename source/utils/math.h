#pragma once

#include "linalg/linalg.h"
#include "hlslpp/hlsl++.h"

using namespace linalg;
using namespace linalg::aliases;

using uint = uint32_t;

#define ENABLE_HLSLPP 1

inline hlslpp::float4 to_hlslpp(const float4& v)
{
    return hlslpp::float4(v.x, v.y, v.z, v.w);
}

inline hlslpp::float4x4 to_hlslpp(const float4x4& m)
{
    return hlslpp::float4x4(to_hlslpp(m[0]), to_hlslpp(m[1]), to_hlslpp(m[2]), to_hlslpp(m[3]));
}

inline float4x4 mul(const float4x4& a, const float4x4& b)
{
#if ENABLE_HLSLPP
    hlslpp::float4x4 a1 = to_hlslpp(a);
    hlslpp::float4x4 b1 = to_hlslpp(b);

    return float4x4(mul(b1, a1).f32_128_0);
#else
    return mul<float, 4>(a, b);
#endif
}

inline float4 mul(const float4x4& a, const float4& b)
{
#if ENABLE_HLSLPP
    hlslpp::float4x4 a1 = to_hlslpp(a);
    hlslpp::float4 b1 = to_hlslpp(b);

    return float4(hlslpp::mul(b1, a1).f32);
#else
    return mul<float, 4>(a, b);
#endif
}

inline float4x4 inverse(const float4x4& m)
{
#if ENABLE_HLSLPP
    hlslpp::float4x4 m1 = to_hlslpp(m);
    return float4x4(hlslpp::inverse(m1).f32_128_0);
#else
    return inverse<float, 4>(m);
#endif
}

template<class T>
inline T degree_to_randian(T degree)
{
    const float PI = 3.141592653f;
    return degree * PI / 180.0f;
}

template<class T>
inline T radian_to_degree(T radian)
{
    const float PI = 3.141592653f;
    return radian * 180.0f / PI;
}

inline float4 rotation_quat(const float3& euler_angles) //pitch-yaw-roll order, in degrees
{
    float3 radians = degree_to_randian(euler_angles);

    float cx = std::cos(radians.x * 0.5f);
    float sx = std::sin(radians.x * 0.5f);
    float cy = std::cos(radians.y * 0.5f);
    float sy = std::sin(radians.y * 0.5f);
    float cz = std::cos(radians.z * 0.5f);
    float sz = std::sin(radians.z * 0.5f);

    const float4 q
    {
        cx * sy * sz + sx * cy * cz,
        cx * sy * cz - sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz
    };

    return q;
}

inline float4x4 ortho_matrix(float l, float r, float b, float t, float n, float f)
{
    //https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-d3dxmatrixorthooffcenterlh
    return { {2 / (r - l), 0, 0, 0}, {0, 2 / (t - b), 0, 0}, {0, 0, 1 / (f - n), 0}, {(l + r) / (l - r), (t + b) / (b - t), n / (n - f), 1} };
}

inline void decompose(const float4x4& matrix, float3& translation, float3& rotation, float3& scale)
{
    translation = matrix[3].xyz();

    float3 right = matrix[0].xyz();
    float3 up = matrix[1].xyz();
    float3 dir = matrix[2].xyz();

    scale[0] = length(right);
    scale[1] = length(up);
    scale[2] = length(dir);

    right = normalize(right);
    up = normalize(up);
    dir = normalize(dir);

    rotation[0] = atan2f(up[2], dir[2]);
    rotation[1] = atan2f(-right[2], sqrtf(up[2] * up[2] + dir[2] * dir[2]));
    rotation[2] = atan2f(right[1], right[0]);

    rotation = radian_to_degree(rotation);
}

inline void decompose(const float4x4& matrix, float3& translation, float4& rotation, float3& scale)
{
    float3 rotation_angles;
    decompose(matrix, translation, rotation_angles, scale);

    rotation = rotation_quat(rotation_angles);
}

inline bool nearly_equal(float a, float b)
{
    return std::abs(a - b) < FLT_EPSILON;
}

template<class T>
inline bool nearly_equal(const T& a, const T& b)
{
    return nearly_equal(a.x, b.x) && nearly_equal(a.y, b.y) && nearly_equal(a.z, b.z) && nearly_equal(a.w, b.w);
}

inline float4 normalize_plane(const float4& plane)
{
    float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    return plane * (1.0f / length);
}

inline bool FrustumCull(const float4* planes, uint32_t plane_count, float3 center, float radius)
{
    for (uint32_t i = 0; i < plane_count; i++)
    {
        if (dot(center, planes[i].xyz()) + planes[i].w + radius < 0)
        {
            return false;
        }
    }

    return true;
}