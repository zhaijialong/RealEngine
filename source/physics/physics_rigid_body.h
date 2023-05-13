#pragma once

#include "physics_defines.h"
#include "utils/math.h"

class IPhysicsShape;

class IPhysicsRigidBody
{
public:
    virtual ~IPhysicsRigidBody() {}

    virtual void AddToPhysicsSystem(bool activate) = 0;
    virtual void RemoveFromPhysicsSystem() = 0;

    virtual void Activate() = 0;
    virtual void Deactivate() = 0;
    virtual bool IsActive() const = 0;

    virtual void SetShape(IPhysicsShape* shape) = 0;

    virtual uint16_t GetLayer() const = 0;
    virtual void SetLayer(uint16_t layer) = 0;

    virtual void* GetUserData() const = 0;

    virtual PhysicsMotion GetMotionType() const = 0;
    virtual void SetMotionType(PhysicsMotion motion_type) = 0;

    virtual float3 GetScale() const = 0;
    virtual void SetScale(const float3& scale) = 0;

    virtual float3 GetPosition() const = 0;
    virtual void SetPosition(const float3& position) = 0;

    virtual quaternion GetRotation() const = 0;
    virtual void SetRotation(const quaternion& rotation) = 0;

    virtual void SetPositionAndRotation(const float3& position, const quaternion& rotation) = 0;

    virtual float3 GetLinearVelocity() const = 0;
    virtual void SetLinearVelocity(const float3& velocity) = 0;

    virtual float3 GetAngularVelocity() const = 0;
    virtual void SetAngularVelocity(const float3& velocity) = 0;

    virtual float GetFriction() const = 0;
    virtual void SetFriction(float friction) = 0;

    virtual void AddForce(const float3& force) = 0;
    virtual void AddImpulse(const float3& impulse) = 0;
    virtual void AddTorque(const float3& torque) = 0;
};