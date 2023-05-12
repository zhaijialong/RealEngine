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
    bool CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, bool winding_order_ccw);
    bool CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count, bool winding_order_ccw);
    bool CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, bool winding_order_ccw);

    JPH::Shape* GetShape() const { return m_shape.GetPtr(); }

    virtual bool IsConvexShape() const override;
    virtual float GetDensity() const override;
    virtual void SetDensity(float density) override;

private:
    JPH::Ref<JPH::Shape> m_shape;
};