#include "physics_system.h"
#include "utils/log.h"
#include "utils/memory.h"
#include <cstdarg>

#include "Jolt/Jolt.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"

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

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
}

void PhysicsSystem::Initialize()
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

void PhysicsSystem::Tick(float delta_time)
{
}
