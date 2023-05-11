#include "jolt_system.h"
#include "jolt_broad_phase_layer.h"
#include "jolt_layer_filter.h"
#include "jolt_body_activation_listener.h"
#include "jolt_contact_listener.h"
#include "jolt_debug_renderer.h"
#include "jolt_job_system.h"
#include "jolt_shape.h"
#include "jolt_rigid_body.h"
#include "core/engine.h"
#include "renderer/renderer.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/profiler.h"

#include "Jolt/Jolt.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"

static void JoltTraceImpl(const char* inFMT, ...)
{
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    RE_INFO(buffer);
}

static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
    RE_ERROR("{}:{}: ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true;
};

const JPH::uint cMaxBodies = 65536;
const JPH::uint cNumBodyMutexes = 0;
const JPH::uint cMaxBodyPairs = 65536;
const JPH::uint cMaxContactConstraints = 10240;

JoltSystem::JoltSystem()
{
    m_pRenderer = Engine::GetInstance()->GetRenderer();

#if 1
    JPH::Allocate = RE_ALLOC;
    JPH::Free = RE_FREE;
    JPH::AlignedAllocate = RE_ALLOC;
    JPH::AlignedFree = RE_FREE;
#else
    JPH::RegisterDefaultAllocator();
#endif

    JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

JoltSystem::~JoltSystem()
{
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
}

void JoltSystem::Initialize()
{
    m_pTempAllocator = eastl::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    //m_pJobSystem = eastl::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    m_pJobSystem = eastl::make_unique<JoltJobSystem>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
    m_pBroadPhaseLayer = eastl::make_unique<JoltBroadPhaseLayerInterface>();
    m_pBroadPhaseLayerFilter = eastl::make_unique<JoltObjectVsBroadPhaseLayerFilter>();
    m_pObjectLayerFilter = eastl::make_unique<JoltObjectLayerPairFilter>();
    m_pBodyActivationListener = eastl::make_unique<JoltBodyActivationListener>();
    m_pContactListener = eastl::make_unique<JoltContactListener>();
#ifdef JPH_DEBUG_RENDERER
    m_pDebugRenderer = eastl::make_unique<JoltDebugRenderer>(m_pRenderer);
#endif

    m_pSystem = eastl::make_unique<JPH::PhysicsSystem>();
    m_pSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *m_pBroadPhaseLayer, *m_pBroadPhaseLayerFilter, *m_pObjectLayerFilter);
    m_pSystem->SetBodyActivationListener(m_pBodyActivationListener.get());
    m_pSystem->SetContactListener(m_pContactListener.get());
}

void JoltSystem::OptimizeTLAS()
{
    m_pSystem->OptimizeBroadPhase();
}

void JoltSystem::Tick(float delta_time)
{
    CPU_EVENT("Physics", "JoltSystem::Tick");

    const float cDeltaTime = 1.0f / 60.0f;
    const int cCollisionSteps = 1;
    const int cIntegrationSubSteps = 1;

    m_pSystem->Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, m_pTempAllocator.get(), m_pJobSystem.get());

    if (m_pRenderer->GetOutputType() == RendererOutput::Physics)
    {
        CPU_EVENT("Physics", "Debug Draw");

#ifdef JPH_DEBUG_RENDERER
        JPH::BodyManager::DrawSettings drawSettings;
        m_pSystem->DrawBodies(drawSettings, m_pDebugRenderer.get());

        m_pSystem->DrawConstraints(m_pDebugRenderer.get());
#endif
    }
}

IPhysicsShape* JoltSystem::CreateBoxShape(const float3& half_extent)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateBox(half_extent))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateSphereShape(float radius)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateSphere(radius))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateCapsuleShape(float half_height, float radius)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateCapsule(half_height, radius))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateCylinderShape(float half_height, float radius)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateCylinder(half_height, radius))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateConvexHullShape(eastl::span<float3> points)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateConvexHull(points))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, bool winding_order_ccw)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateMesh(vertices, vertex_stride, vertex_count, winding_order_ccw))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count, bool winding_order_ccw)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateMesh(vertices, vertex_stride, vertex_count, indices, index_count, winding_order_ccw))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsShape* JoltSystem::CreateMeshShape(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, bool winding_order_ccw)
{
    JoltShape* shape = new JoltShape();
    if (!shape->CreateMesh(vertices, vertex_stride, vertex_count, indices, index_count, winding_order_ccw))
    {
        delete shape;
        return nullptr;
    }
    return shape;
}

IPhysicsRigidBody* JoltSystem::CreateRigidBody(const IPhysicsShape* shape, const float3& position, const quaternion& rotation, PhysicsMotion motion_type, uint16_t layer)
{
    JoltRigidBody* rigidBody = new JoltRigidBody(m_pSystem->GetBodyInterface());
    if (!rigidBody->Create(shape, position, rotation, motion_type, layer))
    {
        delete rigidBody;
        return nullptr;
    }
    return rigidBody;
}

void JoltSystem::AddRigidBody(const IPhysicsRigidBody* rigid_body, bool activate)
{
    JPH::BodyInterface& bodyInterface = m_pSystem->GetBodyInterface();
    bodyInterface.AddBody(((JoltRigidBody*)rigid_body)->GetID(), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void JoltSystem::RemoveRigidBody(const IPhysicsRigidBody* rigid_body)
{
    JPH::BodyInterface& bodyInterface = m_pSystem->GetBodyInterface();
    bodyInterface.RemoveBody(((JoltRigidBody*)rigid_body)->GetID());
}
