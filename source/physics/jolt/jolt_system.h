#pragma once

#include "../physics_system.h"
#include "EASTL/unique_ptr.h"

namespace JPH
{
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemWithBarrier;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class BodyActivationListener;
    class ContactListener;
}

class Renderer;

class JoltSystem : public IPhysicsSystem
{
public:
    JoltSystem();
    ~JoltSystem();

    virtual void Initialize() override;
    virtual void OptimizeTLAS() override;
    virtual void Tick(float delta_time) override;

    virtual IPhysicsShape* CreateBoxShape(const float3& half_extent) override;
    virtual IPhysicsShape* CreateSphereShape(float radius) override;
    virtual IPhysicsShape* CreateCapsuleShape(float half_height, float radius) override;
    virtual IPhysicsShape* CreateCylinderShape(float half_height, float radius) override;
    virtual IPhysicsShape* CreateConvexHullShape(eastl::span<float3> points) override;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, bool winding_order_ccw = false) override;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count, bool winding_order_ccw = false) override;
    virtual IPhysicsShape* CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, bool winding_order_ccw = false) override;

    virtual IPhysicsRigidBody* CreateRigidBody(const IPhysicsShape* shape, PhysicsMotion motion_type, uint16_t layer) override;

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<class JPH::PhysicsSystem> m_pSystem;
    eastl::unique_ptr<class JPH::TempAllocatorImpl> m_pTempAllocator;
    eastl::unique_ptr<class JPH::BroadPhaseLayerInterface> m_pBroadPhaseLayer;
    eastl::unique_ptr<class JPH::ObjectVsBroadPhaseLayerFilter> m_pBroadPhaseLayerFilter;
    eastl::unique_ptr<class JPH::ObjectLayerPairFilter> m_pObjectLayerFilter;
    eastl::unique_ptr<class JPH::BodyActivationListener> m_pBodyActivationListener;
    eastl::unique_ptr<class JPH::ContactListener> m_pContactListener;
    eastl::unique_ptr<class JPH::JobSystemWithBarrier> m_pJobSystem;
    eastl::unique_ptr<class JoltDebugRenderer> m_pDebugRenderer;
};