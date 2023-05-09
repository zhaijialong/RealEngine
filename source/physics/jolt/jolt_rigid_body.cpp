#include "jolt_rigid_body.h"
#include "jolt_shape.h"
#include "jolt_utils.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/Body.h"

JoltRigidBody::JoltRigidBody(JPH::BodyInterface& bodyInterface) : 
    m_bodyInterface(bodyInterface)
{
}

JoltRigidBody::~JoltRigidBody()
{
    m_bodyInterface.DestroyBody(m_bodyID);
}

bool JoltRigidBody::Create(const IPhysicsShape* shape, const float3& position, const quaternion& rotation, PhysicsMotion motion_type, uint16_t layer)
{
    JPH::BodyCreationSettings settings(
        ((const JoltShape*)shape)->GetShape(),
        ToJolt(position),
        ToJolt(rotation),
        ToJolt(motion_type),
        ToJolt(layer));

    JPH::Body* body = m_bodyInterface.CreateBody(settings);
    if (body == nullptr)
    {
        return false;
    }

    m_bodyID = body->GetID();
    return true;
}
