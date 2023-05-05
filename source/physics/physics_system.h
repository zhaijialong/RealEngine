#pragma once

class IPhysicsSystem
{
public:
    virtual ~IPhysicsSystem() {}

    virtual void Initialize() = 0;
    virtual void OptimizeTLAS() = 0;
    virtual void Tick(float delta_time) = 0;
};