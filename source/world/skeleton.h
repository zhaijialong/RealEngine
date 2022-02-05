#pragma once

#include "utils/math.h"

class Skeleton
{
public:
    Skeleton(const std::string& name);


private:
    std::vector<uint32_t> m_joints;
    std::vector<float4x4> m_inverseBindMatrices;
};