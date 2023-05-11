#pragma once

#include "physics_defines.h"
#include "utils/math.h"
#include "EASTL/span.h"

class IPhysicsShape;
class IPhysicsRigidBody;

class IPhysicsSystem
{
public:
    virtual ~IPhysicsSystem() {}

    virtual void Initialize() = 0;
    virtual void OptimizeTLAS() = 0;
    virtual void Tick(float delta_time) = 0;

    virtual IPhysicsShape* CreateBoxShape(const float3& half_extent) = 0;
    virtual IPhysicsShape* CreateSphereShape(float radius) = 0;
    virtual IPhysicsShape* CreateCapsuleShape(float half_height, float radius) = 0;
    virtual IPhysicsShape* CreateCylinderShape(float half_height, float radius) = 0;
    virtual IPhysicsShape* CreateConvexHullShape(eastl::span<float3> points) = 0;
    //todo : virtual IPhysicsShape* CreateCompoundShape() = 0;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, bool winding_order_ccw = false) = 0;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count, bool winding_order_ccw = false) = 0;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, bool winding_order_ccw = false) = 0;
    //todo : virtual IPhysicsShape* CreateHeightFiledShape() = 0;
    virtual IPhysicsRigidBody* CreateRigidBody(const IPhysicsShape* shape, const float3& position, const quaternion& rotation, PhysicsMotion motion_type, uint16_t layer) = 0;

    virtual void AddRigidBody(const IPhysicsRigidBody* rigid_body, bool activate) = 0;
    virtual void RemoveRigidBody(const IPhysicsRigidBody* rigid_body) = 0;
};