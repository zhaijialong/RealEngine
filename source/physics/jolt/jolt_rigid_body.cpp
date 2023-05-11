#include "jolt_rigid_body.h"
#include "jolt_shape.h"
#include "jolt_utils.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"

JoltRigidBody::JoltRigidBody(JPH::BodyInterface& bodyInterface) : 
    m_bodyInterface(bodyInterface)
{
}

JoltRigidBody::~JoltRigidBody()
{
    m_bodyInterface.DestroyBody(m_bodyID);
}

bool JoltRigidBody::Create(const IPhysicsShape* shape, PhysicsMotion motion_type, uint16_t layer)
{
    m_shape = ((const JoltShape*)shape)->GetShape();

    JPH::BodyCreationSettings settings;
    settings.SetShape(m_shape);
    settings.mMotionType = ToJolt(motion_type);
    settings.mObjectLayer = ToJolt(layer);

    JPH::Body* body = m_bodyInterface.CreateBody(settings);
    if (body == nullptr)
    {
        return false;
    }

    m_bodyID = body->GetID();
    return true;
}

void JoltRigidBody::AddToPhysicsSystem(bool activate)
{
    m_bodyInterface.AddBody(m_bodyID, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void JoltRigidBody::RemoveFromPhysicsSystem()
{
    m_bodyInterface.RemoveBody(m_bodyID);
}

float3 JoltRigidBody::GetScale() const
{
    return m_scale;
}

void JoltRigidBody::SetScale(const float3& scale)
{
    if (!nearly_equal(m_scale.x, scale.x) ||
        !nearly_equal(m_scale.y, scale.y) ||
        !nearly_equal(m_scale.z, scale.z))
    {
        m_scale = scale;

        if (m_scale == float3(1.0f, 1.0f, 1.0f))
        {
            m_bodyInterface.SetShape(m_bodyID, m_shape, true, JPH::EActivation::DontActivate);
        }
        else
        {
            m_bodyInterface.SetShape(m_bodyID, new JPH::ScaledShape(m_shape, ToJolt(m_scale)), true, JPH::EActivation::DontActivate);
        }
    }
}

float3 JoltRigidBody::GetPosition() const
{
    return FromJolt(m_bodyInterface.GetPosition(m_bodyID));
}

void JoltRigidBody::SetPosition(const float3& position)
{
    m_bodyInterface.SetPosition(m_bodyID, ToJolt(position), JPH::EActivation::DontActivate);
}

quaternion JoltRigidBody::GetRotation() const
{
    return FromJolt(m_bodyInterface.GetRotation(m_bodyID));
}

void JoltRigidBody::SetRotation(const quaternion& rotation)
{
    m_bodyInterface.SetRotation(m_bodyID, ToJolt(rotation), JPH::EActivation::DontActivate);
}
