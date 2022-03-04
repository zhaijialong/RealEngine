#include "skeleton.h"
#include "skeletal_mesh.h"
#include "core/engine.h"

Skeleton::Skeleton(const eastl::string& name)
{
    m_name = name;
    m_pRenderer = Engine::GetInstance()->GetRenderer();
}

void Skeleton::Update(const SkeletalMesh* mesh)
{
    for (size_t i = 0; i < m_joints.size(); ++i)
    {
        const SkeletalMeshNode* node = mesh->GetNode(m_joints[i]);

        m_jointMatrices[i] = mul(node->globalTransform, m_inverseBindMatrices[i]);
    }

    m_jointMatricesAddress = m_pRenderer->AllocateSceneConstant(m_jointMatrices.data(), sizeof(float4x4) * (uint32_t)m_jointMatrices.size());
}
