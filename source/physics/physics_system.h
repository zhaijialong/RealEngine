#pragma once

#include "physics_layer.h"
#include "EASTL/unique_ptr.h"

namespace JPH
{
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class BodyActivationListener;
    class ContactListener;
}

class PhysicsSystem
{
public:
    PhysicsSystem();
    ~PhysicsSystem();

    void Initialize();
    void OptimizeBVH();
    void Tick(float delta_time);

private:
    eastl::unique_ptr<class JPH::PhysicsSystem> m_pJoltSystem;
    eastl::unique_ptr<class JPH::TempAllocatorImpl> m_pTempAllocator;
    eastl::unique_ptr<class JPH::BroadPhaseLayerInterface> m_pBroadPhaseLayer;
    eastl::unique_ptr<class JPH::ObjectVsBroadPhaseLayerFilter> m_pBroadPhaseLayerFilter;
    eastl::unique_ptr<class JPH::ObjectLayerPairFilter> m_pObjectLayerFilter;
    eastl::unique_ptr<class JPH::BodyActivationListener> m_pBodyActivationListener;
    eastl::unique_ptr<class JPH::ContactListener> m_pContactListener;
    eastl::unique_ptr<class JPH::JobSystemThreadPool> m_pJobSystem; //todo : use a custom job system built upon enkiTS
};