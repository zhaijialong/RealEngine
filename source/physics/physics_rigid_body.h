#pragma once

#include "utils/math.h"

class IPhysicsRigidBody
{
public:
    virtual ~IPhysicsRigidBody() {}

    virtual void AddToPhysicsSystem(bool activate) = 0;
    virtual void RemoveFromPhysicsSystem() = 0;

    virtual float3 GetScale() const = 0;
    virtual void SetScale(const float3& scale) = 0;

    virtual float3 GetPosition() const = 0;
    virtual void SetPosition(const float3& position) = 0;

    virtual quaternion GetRotation() const = 0;
    virtual void SetRotation(const quaternion& rotation) = 0;
};