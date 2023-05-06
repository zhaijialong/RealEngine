#include "physics.h"
#include "jolt/jolt_system.h"

IPhysicsSystem* CreatePhysicsSystem(PhysicsEngine physicsEngine)
{
    switch (physicsEngine)
    {
    case PhysicsEngine::Jolt:
        return new JoltSystem();
    default:
        break;
    }
    return nullptr;
}
