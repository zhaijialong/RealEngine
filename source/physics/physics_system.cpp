#include "physics_system.h"
#include "physics_broad_phase_layer.h"
#include "physics_layer_filter.h"
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

PhysicsSystem::PhysicsSystem()
{
    JPH::Allocate = RE_ALLOC;
    JPH::Free = RE_FREE;
    JPH::AlignedAllocate = RE_ALLOC;
    JPH::AlignedFree = RE_FREE;
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

    m_pJoltSystem = eastl::make_unique<JPH::PhysicsSystem>();
    m_pJoltSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *m_pBroadPhaseLayer, *m_pBroadPhaseLayerFilter, *m_pObjectLayerFilter);

    //todo
    //m_pJoltSystem->SetBodyActivationListener
    //m_pJoltSystem->SetContactListener
}

void PhysicsSystem::OptimizeBVH()
{
    m_pJoltSystem->OptimizeBroadPhase();
}

void PhysicsSystem::Tick(float delta_time)
{
    const int cCollisionSteps = 1;
    const int cIntegrationSubSteps = 1;

    m_pJoltSystem->Update(delta_time, cCollisionSteps, cIntegrationSubSteps, m_pTempAllocator.get(), m_pJobSystem.get());
}
