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

    bool Create(const IPhysicsShape* shape, PhysicsMotion motion_type, uint16_t layer);
    JPH::BodyID GetID() const { return m_bodyID; }

    virtual void AddToPhysicsSystem(bool activate) override;
    virtual void RemoveFromPhysicsSystem() override;

    virtual float3 GetScale() const override;
    virtual void SetScale(const float3& scale) override;

    virtual float3 GetPosition() const override;
    virtual void SetPosition(const float3& position) override;

    virtual quaternion GetRotation() const override;
    virtual void SetRotation(const quaternion& rotation) override;

private:
    JPH::BodyInterface& m_bodyInterface;
    JPH::BodyID m_bodyID;
};