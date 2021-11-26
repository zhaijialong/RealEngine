#pragma once

#include "linalg/linalg.h"
#include <vector>

using namespace linalg;
using namespace linalg::aliases;

using uint = uint32_t;

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

inline float3 vector_to_float3(const std::vector<float>& v)
{
    return float3(v[0], v[1], v[2]);
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