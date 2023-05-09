#pragma once

#include "stdint.h"

enum class PhysicsEngine
{
    Jolt,
};

namespace PhysicsLayers
{
    static constexpr uint16_t STATIC(0);
    static constexpr uint16_t DYNAMIC(1);
    static constexpr uint16_t NUM(2);
}

namespace PhysicsBroadPhaseLayers
{
    static constexpr uint8_t STATIC(0);
    static constexpr uint8_t DYNAMIC(1);
    static constexpr uint8_t NUM(2);
}

enum class PhysicsMotion
{
    Static,
    Kinematic,
    Dynamic,
};