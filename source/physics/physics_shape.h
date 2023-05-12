#pragma once

class IPhysicsShape
{
public:
    virtual ~IPhysicsShape() {}

    virtual bool IsConvexShape() const = 0;

    // only available forr convex shapes
    virtual float GetDensity() const = 0;
    virtual void SetDensity(float density) = 0;
};