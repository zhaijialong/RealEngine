#include "jolt_layer_filter.h"
#include "../physics_defines.h"
#include "utils/assert.h"

bool JoltObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const
{
    switch (inLayer1)
    {
    case PhysicsLayers::STATIC:
        return (uint8_t)inLayer2 == PhysicsBroadPhaseLayers::DYNAMIC;
    case PhysicsLayers::DYNAMIC:
        return true;
    default:
        RE_ASSERT(false);
        return false;
    }
}

bool JoltObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const
{
    switch (inLayer1)
    {
    case PhysicsLayers::STATIC:
        return inLayer2 == PhysicsLayers::DYNAMIC; // Non moving only collides with moving
    case PhysicsLayers::DYNAMIC:
        return true; // Moving collides with everything
    default:
        RE_ASSERT(false);
        return false;
    }
}
