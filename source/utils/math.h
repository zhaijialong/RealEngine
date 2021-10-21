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

inline float4x4 RightHandToLeftHand(const float4x4& matrix)
{
	return float4x4(float4(matrix.x.x, matrix.x.y, -matrix.x.z, matrix.x.w),
		float4(matrix.y.x, matrix.y.y, -matrix.y.z, matrix.y.w),
		float4(-matrix.z.x, -matrix.z.y, matrix.z.z, -matrix.z.w),
		matrix.w);
}
