#pragma once

#include "../physics_rigid_body.h"
#include "../physics_defines.h"
#include "utils/math.h"
#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"

class IPhysicsShape;

class JoltRigidBody : public IPhysicsRigidBody
{
public:
    JoltRigidBody(JPH::BodyInterface& bodyInterface);
    ~JoltRigidBody();

    bool Create(const IPhysicsShape* shape, PhysicsMotion motion_type, uint16_t layer, void* user_data);
    JPH::BodyID GetID() const { return m_bodyID; }

    virtual void AddToPhysicsSystem(bool activate) override;
    virtual void RemoveFromPhysicsSystem() override;

    virtual void Activate() override;
    virtual void Deactivate() override;
    virtual bool IsActive() const override;

    virtual void SetShape(IPhysicsShape* shape) override;

    virtual uint16_t GetLayer() const override;
    virtual void SetLayer(uint16_t layer) override;

    virtual void* GetUserData() const override;

    virtual PhysicsMotion GetMotionType() const override;
    virtual void SetMotionType(PhysicsMotion motion_type) override;

    virtual float3 GetScale() const override;
    virtual void SetScale(const float3& scale) override;

    virtual float3 GetPosition() const override;
    virtual void SetPosition(const float3& position) override;

    virtual quaternion GetRotation() const override;
    virtual void SetRotation(const quaternion& rotation) override;

    virtual void SetPositionAndRotation(const float3& position, const quaternion& rotation) override;

    virtual float3 GetLinearVelocity() const override;
    virtual void SetLinearVelocity(const float3& velocity) override;

    virtual float3 GetAngularVelocity() const override;
    virtual void SetAngularVelocity(const float3& velocity) override;

    virtual float GetFriction() const override;
    virtual void SetFriction(float friction) override;

    virtual void AddForce(const float3& force) override;
    virtual void AddImpulse(const float3& impulse) override;
    virtual void AddTorque(const float3& torque) override;

private:
    JPH::BodyInterface& m_bodyInterface;
    JPH::BodyID m_bodyID;

    float3 m_scale{ 1.0f, 1.0f, 1.0f };
    JPH::Ref<JPH::Shape> m_shape;
};