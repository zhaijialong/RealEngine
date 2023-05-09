#pragma once

#include "../physics_shape.h"
#include "utils/math.h"
#include "EASTL/span.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"

class JoltShape : public IPhysicsShape
{
public:
    bool CreateBox(const float3& half_extent);
    bool CreateSphere(float radius);
    bool CreateCapsule(float half_height, float radius);
    bool CreateCylinder(float half_height, float radius);
    bool CreateConvexHull(eastl::span<float3> points);

    JPH::Shape* GetShape() const { return m_shape.GetPtr(); }

private:
    JPH::Ref<JPH::Shape> m_shape;
};