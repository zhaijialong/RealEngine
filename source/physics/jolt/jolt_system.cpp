#include "jolt_system.h"
#include "jolt_broad_phase_layer.h"
#include "jolt_layer_filter.h"
#include "jolt_body_activation_listener.h"
#include "jolt_contact_listener.h"
#include "jolt_debug_renderer.h"
#include "jolt_job_system.h"
#include "../physics_defines.h"
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

//should be removed later
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

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
    if (inMessage)
    {
        RE_ERROR("{}:{}: ({}) {}", inFile, inLine, inExpression, inMessage);
	}
    else
    {
        RE_ERROR("{}:{}: ({})", inFile, inLine, inExpression);
    }

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

    ///////// testing code here, should be removed later /////////
    using namespace JPH;
    BodyInterface& body_interface = m_pSystem->GetBodyInterface();

    BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
    ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    ShapeRefC floor_shape = floor_shape_result.Get();
    BodyCreationSettings floor_settings(floor_shape, RVec3(0.0, -1.0, 0.0), Quat::sIdentity(), EMotionType::Static, (ObjectLayer)PhysicsLayers::STATIC);
    body_interface.CreateAndAddBody(floor_settings, EActivation::DontActivate);

    BodyCreationSettings sphere_settings(new SphereShape(5.f), RVec3(0.0, 25.0, 0.0), Quat::sIdentity(), EMotionType::Dynamic, (ObjectLayer)PhysicsLayers::DYNAMIC);
    body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);
}

void JoltSystem::OptimizeTLAS()
{
    m_pSystem->OptimizeBroadPhase();
}

void JoltSystem::Tick(float delta_time)
{
    CPU_EVENT("Tick", "Physics");

    const float cDeltaTime = 1.0f / 60.0f;
    const int cCollisionSteps = 1;
    const int cIntegrationSubSteps = 1;

    m_pSystem->Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, m_pTempAllocator.get(), m_pJobSystem.get());

    if (m_pRenderer->GetOutputType() == RendererOutput::Physics)
    {        
#ifdef JPH_DEBUG_RENDERER
        JPH::BodyManager::DrawSettings drawSettings;
        m_pSystem->DrawBodies(drawSettings, m_pDebugRenderer.get());

        m_pSystem->DrawConstraints(m_pDebugRenderer.get());
#endif
    }
}
