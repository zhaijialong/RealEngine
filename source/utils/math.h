#pragma once

#include "assert.h"
#include "float.h" // for FLT_EPSILON
#include "linalg/linalg.h"
#include "hlslpp/hlsl++.h"

using namespace linalg;
using namespace linalg::aliases;

using uint = uint32_t;
using quaternion = float4;

static const float PI = 3.14159265f;

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
inline T degree_to_radian(T degree)
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

inline quaternion rotation_quat(const float3& euler_angles) //pitch-yaw-roll order, in degrees
{
    float3 radians = degree_to_radian(euler_angles);

    float3 c = cos(radians * 0.5f);
    float3 s = sin(radians * 0.5f);

    quaternion q;
    q.w = c.x * c.y * c.z + s.x * s.y * s.z;
    q.x = s.x * c.y * c.z - c.x * s.y * s.z;
    q.y = c.x * s.y * c.z + s.x * c.y * s.z;
    q.z = c.x * c.y * s.z - s.x * s.y * c.z;

    return q;
}

inline float rotation_pitch(const quaternion& q)
{
    //return T(atan(T(2) * (q.y * q.z + q.w * q.x), q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z));
    const float y = 2.0f * (q.y * q.z + q.w * q.x);
    const float x = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
    if (y == 0.0f && x == 0.0f) //avoid atan2(0,0) - handle singularity
    {
        return radian_to_degree(2.0f * atan2(q.x, q.w));
    }

    return radian_to_degree(atan2(y, x));
}

inline float rotation_yaw(const quaternion& q)
{
    return radian_to_degree(asin(clamp(-2.0f * (q.x * q.z - q.w * q.y), -1.0f, 1.0f)));
}

inline float rotation_roll(const quaternion& q)
{
    return radian_to_degree(atan2(2.0f * (q.x * q.y + q.w * q.z), q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z));
}

inline float3 rotation_angles(const quaternion& q)
{
    return float3(rotation_pitch(q), rotation_yaw(q), rotation_roll(q));
}

inline float normalize_angle(float degree)
{
    degree = fmodf(degree, 360.0f); // (-360, 360)

    if (degree < 0.0f)
    {
        degree += 360.0f; // [0, 360)
    }

    if (degree > 180.0f)
    {
        degree -= 360.0f; // (-180, 180]
    }

    return degree;
}

inline quaternion rotation_slerp(const quaternion& a, quaternion b, float interpolationValue)
{
    float dotProduct = dot(a, b);

    //make sure we take the shortest path in case dot Product is negative
    if (dotProduct < 0.0)
    {
        b = -b;
        dotProduct = -dotProduct;
    }

    //if the two quaternions are too close to each other, just linear interpolate between the 4D vector
    if (dotProduct > 0.9995)
    {
        return normalize(a + interpolationValue * (b - a));
    }

    //perform the spherical linear interpolation
    float theta_0 = acos(dotProduct);
    float theta = interpolationValue * theta_0;
    float sin_theta = sin(theta);
    float sin_theta_0 = sin(theta_0);

    float scalePreviousQuat = cos(theta) - dotProduct * sin_theta / sin_theta_0;
    float scaleNextQuat = sin_theta / sin_theta_0;
    return scalePreviousQuat * a + scaleNextQuat * b;
}

inline float4x4 ortho_matrix(float l, float r, float b, float t, float n, float f)
{
    //https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-d3dxmatrixorthooffcenterlh
    return { {2 / (r - l), 0, 0, 0}, {0, 2 / (t - b), 0, 0}, {0, 0, 1 / (f - n), 0}, {(l + r) / (l - r), (t + b) / (b - t), n / (n - f), 1} };
}

inline void decompose(const float4x4& matrix, float3& translation, quaternion& rotation, float3& scale)
{
    translation = matrix[3].xyz();

    float3 right = matrix[0].xyz();
    float3 up = matrix[1].xyz();
    float3 dir = matrix[2].xyz();

    scale[0] = length(right);
    scale[1] = length(up);
    scale[2] = length(dir);

    float3x3 rotation_mat = float3x3(right / scale[0], up / scale[1], dir / scale[2]);
    rotation = rotation_quat(rotation_mat);
}

inline void decompose(const float4x4& matrix, float3& translation, float3& rotation, float3& scale)
{
    quaternion q;
    decompose(matrix, translation, q, scale);

    rotation = rotation_angles(q);
}

inline float sign(float x)
{
    return float((x > 0) - (x < 0));
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

// https://bartwronski.com/2017/04/13/cull-that-cone/
inline float4 ConeBoundingSphere(float3 origin, float3 forward, float radius, float halfAngle)
{
    float4 boundingSphere;
    if (halfAngle > PI / 4.0f)
    {
        boundingSphere = float4(origin + cos(halfAngle) * radius * forward, sin(halfAngle) * radius);
    }
    else
    {
        boundingSphere = float4(origin + radius / (2.0f * cos(halfAngle)) * forward, radius / (2.0f * cos(halfAngle)));
    }

    return boundingSphere;
}

inline uint32_t DivideRoudingUp(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}

inline bool IsPow2(uint32_t x)
{
    return (x & (x - 1)) == 0;
}

inline uint32_t RoundUpPow2(uint32_t a, uint32_t b)
{
    RE_ASSERT(IsPow2(b));
    return (a + b - 1) & ~(b - 1);
}

union FP32
{
    uint u;
    float f;
    struct
    {
        uint Mantissa : 23;
        uint Exponent : 8;
        uint Sign : 1;
    };
};

union FP16
{
    uint16_t u;
    struct
    {
        uint Mantissa : 10;
        uint Exponent : 5;
        uint Sign : 1;
    };
};

//https://gist.github.com/rygorous/2156668, float_to_half_fast3
inline uint16_t FloatToHalf(float value)
{
    FP32 f;
    f.f = value;
    FP32 f32infty = { 255 << 23 };
    FP32 f16infty = { 31 << 23 };
    FP32 magic = { 15 << 23 };
    uint sign_mask = 0x80000000u;
    uint round_mask = ~0xfffu;
    FP16 o = { 0 };

    uint sign = f.u & sign_mask;
    f.u ^= sign;

    // NOTE all the integer compares in this function can be safely
    // compiled into signed compares since all operands are below
    // 0x80000000. Important if you want fast straight SSE2 code
    // (since there's no unsigned PCMPGTD).

    if (f.u >= f32infty.u) // Inf or NaN (all exponent bits set)
    {
        o.u = (f.u > f32infty.u) ? 0x7e00 : 0x7c00; // NaN->qNaN and Inf->Inf
    }
    else // (De)normalized number or zero
    {
        f.u &= round_mask;
        f.f *= magic.f;
        f.u -= round_mask;
        if (f.u > f16infty.u) f.u = f16infty.u; // Clamp to signed infinity if overflowed

        o.u = f.u >> 13; // Take the bits!
    }

    o.u |= sign >> 16;
    return o.u;
}

inline float HalfToFloat(uint16_t value)
{
    FP16 h = { value };
    static const FP32 magic = { 113 << 23 };
    static const uint shifted_exp = 0x7c00 << 13; // exponent mask after shift
    FP32 o;

    o.u = (h.u & 0x7fff) << 13;     // exponent/mantissa bits
    uint exp = shifted_exp & o.u;   // just the exponent
    o.u += (127 - 15) << 23;        // exponent adjust

    // handle exponent special cases
    if (exp == shifted_exp) // Inf/NaN?
    {
        o.u += (128 - 16) << 23;    // extra exp adjust
    }
    else if (exp == 0) // Zero/Denormal?
    {
        o.u += 1 << 23;             // extra exp adjust
        o.f -= magic.f;             // renormalize
    }

    o.u |= (h.u & 0x8000) << 16;    // sign bit
    return o.f;
}

inline float asfloat(uint32_t value)
{
    return *((float*)&value);
}

inline uint32_t asuint(float value)
{
    return *((uint32_t*)&value);
}

inline float4 UnpackRGBA8Unorm(uint packed)
{
    return float4(((packed >> 0) & 0xFF) / 255.0f,
        ((packed >> 8) & 0xFF) / 255.0f,
        ((packed >> 16) & 0xFF) / 255.0f,
        ((packed >> 24) & 0xFF) / 255.0f);
}

inline uint PackRGBA8Unorm(float4 input)
{
    byte4 unpacked = byte4(input.x * 255.0 + 0.5,
        input.y * 255.0 + 0.5,
        input.z * 255.0 + 0.5,
        input.w * 255.0 + 0.5);

    return (unpacked.w << 24) | (unpacked.z << 16) | (unpacked.y << 8) | unpacked.x;
}