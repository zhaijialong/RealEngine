#pragma once

#include "linalg/linalg.h"
#include <vector>

using namespace linalg;
using namespace linalg::aliases;

using uint = uint32_t;

inline float degree_to_randian(float degree)
{
	const float PI = 3.141592653f;
	return degree * PI / 180.0f;
}

inline float3 degree_to_randian(float3 degree)
{
	const float PI = 3.141592653f;
	return degree * PI / 180.0f;
}

inline float radian_to_degree(float radian)
{
	const float PI = 3.141592653f;
	return radian * 180.0f / PI;
}

inline float3 radian_to_degree(float3 radian)
{
	const float PI = 3.141592653f;
	return radian * 180.0f / PI;
}

inline float3 vector_to_float3(const std::vector<float>& v)
{
	return float3(v[0], v[1], v[2]);
}

template<class T> 
linalg::vec<T, 4> rotation_quat(const vec<T, 3>& euler_angles) //roll-pitch-yaw order
{
	float cr = std::cos(euler_angles[0] * 0.5f);
	float sr = std::sin(euler_angles[0] * 0.5f);
	float cp = std::cos(euler_angles[1] * 0.5f);
	float sp = std::sin(euler_angles[1] * 0.5f);
	float cy = std::cos(euler_angles[2] * 0.5f);
	float sy = std::sin(euler_angles[2] * 0.5f);

	const vec<T, 4> q
	{
		cy * cp * sr - sy * sp * cr,
		sy * cp * sr + cy * sp * cr,
		sy * cp * cr - cy * sp * sr,
		cy * cp * cr + sy * sp * sr
	};

	return q;
}

//https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-d3dxmatrixorthooffcenterlh
inline float4x4 ortho_matrix(float l, float r, float b, float t, float n, float f)
{
	return { {2 / (r - l), 0, 0, 0}, {0, 2 / (t - b), 0, 0}, {0, 0, 1 / (f - n), 0}, {(l + r) / (l - r), (t + b) / (b - t), n / (n - f), 1} };
}