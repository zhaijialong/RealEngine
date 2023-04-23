#pragma once

class PhysicsSystem
{
public:
    PhysicsSystem();
    ~PhysicsSystem();

    void Initialize();

    void Tick(float delta_time);
};