#pragma once

#include "../physics_defines.h"
#include "utils/math.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/MotionType.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"

inline JPH::Vec3 ToJolt(const float3& x)
{
    return JPH::Vec3(x.x, x.y, x.z);
}

inline float3 FromJolt(const JPH::Vec3& x)
{
    return float3(x.GetX(), x.GetY(), x.GetZ());
}

inline JPH::Quat ToJolt(const quaternion& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline quaternion FromJolt(const JPH::Quat& q)
{
    return quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

inline JPH::EMotionType ToJolt(PhysicsMotion motion)
{
    switch (motion)
    {
    case PhysicsMotion::Static:
        return JPH::EMotionType::Static;
    case PhysicsMotion::Kinematic:
        return JPH::EMotionType::Kinematic;
    case PhysicsMotion::Dynamic:
    default:
        return JPH::EMotionType::Dynamic;
    }
}

inline PhysicsMotion FromJolt(JPH::EMotionType motion)
{
    switch (motion)
    {
    case JPH::EMotionType::Static:
        return PhysicsMotion::Static;
    case JPH::EMotionType::Kinematic:
        return PhysicsMotion::Kinematic;
    case JPH::EMotionType::Dynamic:
    default:
        return PhysicsMotion::Dynamic;
    }
}

inline JPH::ObjectLayer ToJolt(uint16_t layer)
{
    return (JPH::ObjectLayer)layer;
}

template <typename T>
void ToJoltTriangles(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const T* indices, uint32_t index_count,
    JPH::VertexList& vertexList, JPH::IndexedTriangleList& triangleList, bool winding_order_ccw)
{
    vertexList.reserve(vertex_count);

    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        JPH::Float3 v;
        memcpy(&v, (const char*)vertices + i * vertex_stride, sizeof(float3));

        vertexList.push_back(v);
    }

    uint32_t triangle_count = index_count / 3;
    triangleList.reserve(triangle_count);

    for (uint32_t i = 0; i < triangle_count; ++i)
    {
        JPH::IndexedTriangle triangle;

        if (winding_order_ccw) // Jolt uses a right handed coordinate system, and needs CW winding order
        {
            triangle.mIdx[0] = (JPH::uint32)indices[i * 3 + 0];
            triangle.mIdx[2] = (JPH::uint32)indices[i * 3 + 1];
            triangle.mIdx[1] = (JPH::uint32)indices[i * 3 + 2];
        }
        else
        {
            triangle.mIdx[0] = (JPH::uint32)indices[i * 3 + 0];
            triangle.mIdx[1] = (JPH::uint32)indices[i * 3 + 1];
            triangle.mIdx[2] = (JPH::uint32)indices[i * 3 + 2];
        }

        triangleList.push_back(triangle);
    }
}
