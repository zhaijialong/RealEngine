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

bool JoltRigidBody::Create(const IPhysicsShape* shape, PhysicsMotion motion_type, uint16_t layer, void* user_data)
{
    m_shape = ((const JoltShape*)shape)->GetShape();

    JPH::BodyCreationSettings settings;
    settings.SetShape(m_shape);
    settings.mMotionType = ToJolt(motion_type);
    settings.mObjectLayer = ToJolt(layer);
    settings.mUserData = (JPH::uint64)user_data;

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

void JoltRigidBody::Activate()
{
    m_bodyInterface.ActivateBody(m_bodyID);
}

void JoltRigidBody::Deactivate()
{
    m_bodyInterface.DeactivateBody(m_bodyID);
}

bool JoltRigidBody::IsActive() const
{
    return m_bodyInterface.IsActive(m_bodyID);
}

void JoltRigidBody::SetShape(IPhysicsShape* shape)
{
    m_shape = ((const JoltShape*)shape)->GetShape();

    m_bodyInterface.SetShape(m_bodyID, m_shape, true, JPH::EActivation::DontActivate);
}

uint16_t JoltRigidBody::GetLayer() const
{
    return m_bodyInterface.GetObjectLayer(m_bodyID);
}

void JoltRigidBody::SetLayer(uint16_t layer)
{
    m_bodyInterface.SetObjectLayer(m_bodyID, layer);
}

void* JoltRigidBody::GetUserData() const
{
    return (void*)m_bodyInterface.GetUserData(m_bodyID);
}

PhysicsMotion JoltRigidBody::GetMotionType() const
{
    return FromJolt(m_bodyInterface.GetMotionType(m_bodyID));
}

void JoltRigidBody::SetMotionType(PhysicsMotion motion_type)
{
    m_bodyInterface.SetMotionType(m_bodyID, ToJolt(motion_type), JPH::EActivation::DontActivate);
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

void JoltRigidBody::SetPositionAndRotation(const float3& position, const quaternion& rotation)
{
    m_bodyInterface.SetPositionAndRotation(m_bodyID, ToJolt(position), ToJolt(rotation), JPH::EActivation::DontActivate);
}

float3 JoltRigidBody::GetLinearVelocity() const
{
    return FromJolt(m_bodyInterface.GetLinearVelocity(m_bodyID));
}

void JoltRigidBody::SetLinearVelocity(const float3& velocity)
{
    m_bodyInterface.SetLinearVelocity(m_bodyID, ToJolt(velocity));
}

float3 JoltRigidBody::GetAngularVelocity() const
{
    return FromJolt(m_bodyInterface.GetAngularVelocity(m_bodyID));
}

void JoltRigidBody::SetAngularVelocity(const float3& velocity)
{
    m_bodyInterface.SetAngularVelocity(m_bodyID, ToJolt(velocity));
}

float JoltRigidBody::GetFriction() const
{
    return m_bodyInterface.GetFriction(m_bodyID);
}

void JoltRigidBody::SetFriction(float friction)
{
    m_bodyInterface.SetFriction(m_bodyID, friction);
}

void JoltRigidBody::AddForce(const float3& force)
{
    m_bodyInterface.AddForce(m_bodyID, ToJolt(force));
}

void JoltRigidBody::AddImpulse(const float3& impulse)
{
    m_bodyInterface.AddImpulse(m_bodyID, ToJolt(impulse));
}

void JoltRigidBody::AddTorque(const float3& torque)
{
    m_bodyInterface.AddTorque(m_bodyID, ToJolt(torque));
}
