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

    virtual IPhysicsRigidBody* CreateRigidBody(const IPhysicsShape* shape, const float3& position, const quaternion& rotation, PhysicsMotion motion_type, uint16_t layer) override;

    virtual void AddRigidBody(const IPhysicsRigidBody* rigid_body, bool activate) override;
    virtual void RemoveRigidBody(const IPhysicsRigidBody* rigid_body) override;

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