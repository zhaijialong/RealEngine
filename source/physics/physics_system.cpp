#include "physics_system.h"
#include "physics_broad_phase_layer.h"
#include "physics_layer_filter.h"
#include "physics_body_activation_listener.h"
#include "physics_contact_listener.h"
#include "physics_debug_renderer.h"
#include "renderer/renderer.h"
#include "utils/log.h"
#include "utils/memory.h"
#include <cstdarg>

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

PhysicsSystem::PhysicsSystem(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
#if 0 //todo : enable it after we switch to the enkiTS job scheduler
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

PhysicsSystem::~PhysicsSystem()
{
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
}

void PhysicsSystem::Initialize()
{
    m_pTempAllocator = eastl::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    m_pJobSystem = eastl::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    m_pBroadPhaseLayer = eastl::make_unique<PhysicsBroadPhaseLayerInterface>();
    m_pBroadPhaseLayerFilter = eastl::make_unique<PhysicsObjectVsBroadPhaseLayerFilter>();
    m_pObjectLayerFilter = eastl::make_unique<PhysicsObjectLayerPairFilter>();
    m_pBodyActivationListener = eastl::make_unique<PhysicsBodyActivationListener>();
    m_pContactListener = eastl::make_unique<PhysicsContactListener>();
    m_pDebugRenderer = eastl::make_unique<PhysicsDebugRenderer>(m_pRenderer);

    m_pJoltSystem = eastl::make_unique<JPH::PhysicsSystem>();
    m_pJoltSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *m_pBroadPhaseLayer, *m_pBroadPhaseLayerFilter, *m_pObjectLayerFilter);
    m_pJoltSystem->SetBodyActivationListener(m_pBodyActivationListener.get());
    m_pJoltSystem->SetContactListener(m_pContactListener.get());
}

void PhysicsSystem::OptimizeBVH()
{
    m_pJoltSystem->OptimizeBroadPhase();
}

void PhysicsSystem::Tick(float delta_time)
{
    const float cDeltaTime = 1.0f / 60.0f;
    const int cCollisionSteps = 1;
    const int cIntegrationSubSteps = 1;

    m_pJoltSystem->Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, m_pTempAllocator.get(), m_pJobSystem.get());


    if (m_pRenderer->GetOutputType() == RendererOutput::Physics)
    {
        m_pDebugRenderer->Clear();
        
        JPH::BodyManager::DrawSettings drawSettings;
        m_pJoltSystem->DrawBodies(drawSettings, m_pDebugRenderer.get());
    }
}

void PhysicsSystem::DebugDraw(IGfxCommandList* pCommandList)
{
    m_pDebugRenderer->Draw(pCommandList);
}
