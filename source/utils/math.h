#pragma once

#include "linalg/linalg.h"

using namespace linalg;
using namespace linalg::aliases;

inline float degree_to_randian(float degree)
{
	const float PI = 3.141592653f;
	return degree * PI / 180.0f;
}

inline float radian_to_degree(float radian)
{
	const float PI = 3.141592653f;
	return radian * 180.0f / PI;
}