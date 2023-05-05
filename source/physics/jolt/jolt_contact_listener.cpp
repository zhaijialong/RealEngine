#include "jolt_contact_listener.h"

JPH::ValidateResult JoltContactListener::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
{
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void JoltContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
}

void JoltContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
}

void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
}
