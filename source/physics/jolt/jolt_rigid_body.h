#pragma once

#include "../physics_rigid_body.h"
#include "../physics_defines.h"
#include "utils/math.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "Jolt/Physics/Body/BodyInterface.h"

class IPhysicsShape;

class JoltRigidBody : public IPhysicsRigidBody
{
public:
    JoltRigidBody(JPH::BodyInterface& bodyInterface);
    ~JoltRigidBody();

    bool Create(const IPhysicsShape* shape, const float3& position, const quaternion& rotation, PhysicsMotion motion_type, uint16_t layer);
    JPH::BodyID GetID() const { return m_bodyID; }

private:
    JPH::BodyInterface& m_bodyInterface;
    JPH::BodyID m_bodyID;
};