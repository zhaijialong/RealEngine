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

inline JPH::Quat ToJolt(const quaternion& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
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

inline JPH::ObjectLayer ToJolt(uint16_t layer)
{
    return (JPH::ObjectLayer)layer;
}
