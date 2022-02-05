#pragma once

#include "utils/math.h"

class SkeletalMesh;
class Renderer;

class Skeleton
{
    friend class GLTFLoader;

public:
    Skeleton(const std::string& name);

    void Update(const SkeletalMesh* mesh);

    uint32_t GetJointMatricesAddress() const { return m_jointMatricesAddress; }

private:
    Renderer* m_pRenderer;
    std::string m_name;
    std::vector<uint32_t> m_joints;
    std::vector<float4x4> m_inverseBindMatrices;

    std::vector<float4x4> m_jointMatrices;
    uint32_t m_jointMatricesAddress;
};