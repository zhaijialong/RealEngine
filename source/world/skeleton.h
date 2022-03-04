#pragma once

#include "utils/math.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

class SkeletalMesh;
class Renderer;

class Skeleton
{
    friend class GLTFLoader;

public:
    Skeleton(const eastl::string& name);

    void Update(const SkeletalMesh* mesh);

    uint32_t GetJointMatricesAddress() const { return m_jointMatricesAddress; }

private:
    Renderer* m_pRenderer;
    eastl::string m_name;
    eastl::vector<uint32_t> m_joints;
    eastl::vector<float4x4> m_inverseBindMatrices;

    eastl::vector<float4x4> m_jointMatrices;
    uint32_t m_jointMatricesAddress;
};