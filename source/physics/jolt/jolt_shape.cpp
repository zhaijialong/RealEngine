#include "jolt_shape.h"
#include "jolt_utils.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"

bool JoltShape::CreateBox(const float3& half_extent)
{
    JPH::BoxShapeSettings settings(ToJolt(half_extent));
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::BoxShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateSphere(float radius)
{
    JPH::SphereShapeSettings settings(radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::SphereShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateCapsule(float half_height, float radius)
{
    JPH::CapsuleShapeSettings settings(half_height, radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::CapsuleShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateCylinder(float half_height, float radius)
{
    JPH::CylinderShapeSettings settings(half_height, radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::CylinderShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateConvexHull(eastl::span<float3> points)
{
    JPH::Array<JPH::Vec3> joltPoints;
    joltPoints.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i)
    {
        joltPoints.push_back(ToJolt(points[i]));
    }

    JPH::ConvexHullShapeSettings settings(joltPoints);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::ConvexHullShape(settings, result);

    return result.IsValid();
}
