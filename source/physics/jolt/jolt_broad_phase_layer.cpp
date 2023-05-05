#include "jolt_broad_phase_layer.h"
#include "../physics_defines.h"
#include "utils/assert.h"

JPH::uint JoltBroadPhaseLayerInterface::GetNumBroadPhaseLayers() const
{
    return PhysicsBroadPhaseLayers::NUM;
}

JPH::BroadPhaseLayer JoltBroadPhaseLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const
{
    switch (inLayer)
    {
    case PhysicsLayers::STATIC:
        return JPH::BroadPhaseLayer(PhysicsBroadPhaseLayers::STATIC);
    case PhysicsLayers::DYNAMIC:
        return JPH::BroadPhaseLayer(PhysicsBroadPhaseLayers::DYNAMIC);
    default:
        RE_ASSERT(false);
        break;
    }

    return JPH::BroadPhaseLayer(PhysicsBroadPhaseLayers::NUM);
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* JoltBroadPhaseLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const
{
    switch ((JPH::BroadPhaseLayer::Type)inLayer)
    {
    case (JPH::BroadPhaseLayer::Type)PhysicsBroadPhaseLayers::STATIC: 
        return "STATIC";
    case (JPH::BroadPhaseLayer::Type)PhysicsBroadPhaseLayers::DYNAMIC: 
        return "DYNAMIC";
    default: 
        RE_ASSERT(false); 
        return "INVALID";
    }
}
#endif