#include "static_mesh.h"
#include "mesh_material.h"
#include "resource_cache.h"
#include "core/engine.h"

StaticMesh::StaticMesh(const std::string& name)
{
    m_name = name;
}

StaticMesh::~StaticMesh()
{
    ResourceCache* cache = ResourceCache::GetInstance();

    cache->RelaseSceneBuffer(m_posBufferAddress);
    cache->RelaseSceneBuffer(m_uvBufferAddress);
    cache->RelaseSceneBuffer(m_normalBufferAddress);
    cache->RelaseSceneBuffer(m_tangentBufferAddress);

    cache->RelaseSceneBuffer(m_meshletBufferAddress);
    cache->RelaseSceneBuffer(m_meshletVerticesBufferAddress);
    cache->RelaseSceneBuffer(m_meshletIndicesBufferAddress);

    cache->RelaseSceneBuffer(m_indexBufferAddress);
}

bool StaticMesh::Create()
{
    return true;
}

void StaticMesh::Tick(float delta_time)
{
    m_mtxPrevWorld = m_mtxWorld;

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 S = scaling_matrix(m_scale);
    m_mtxWorld = mul(T, mul(R, S));

    UpdateConstants();
    m_pMaterial->UpdateConstants();
}

void StaticMesh::UpdateConstants()
{
    m_modelCB.posBufferAddress = m_posBufferAddress;
    m_modelCB.uvBufferAddress = m_uvBufferAddress;
    m_modelCB.normalBufferAddress = m_normalBufferAddress;
    m_modelCB.tangentBufferAddress = m_tangentBufferAddress;

    m_modelCB.scale = max(max(abs(m_scale.x), abs(m_scale.y)), abs(m_scale.z));
    m_modelCB.center = mul(m_mtxWorld, float4(m_center, 1.0)).xyz();
    m_modelCB.radius = m_radius * m_modelCB.scale;

    m_modelCB.mtxWorld = m_mtxWorld;
    m_modelCB.mtxWorldInverseTranspose = transpose(inverse(m_mtxWorld));
    m_modelCB.mtxPrevWorld = m_mtxPrevWorld;
}

void StaticMesh::Render(Renderer* pRenderer)
{
    RenderBatch& bassPassBatch = pRenderer->AddBasePassBatch();
    bassPassBatch.SetBoundingSphere(m_modelCB.center, m_modelCB.radius);
#if 1
    Dispatch(bassPassBatch, m_pMaterial->GetMeshletPSO());
#else
    Draw(bassPassBatch, m_pMaterial->GetPSO());
#endif

    RenderBatch& shadowPassBatch = pRenderer->AddShadowPassBatch();
    Draw(shadowPassBatch, m_pMaterial->GetShadowPSO());

    if (m_nID == pRenderer->GetMouseHitObjectID())
    {
        RenderBatch& outlinePassBatch = pRenderer->AddForwardPassBatch();
        Draw(outlinePassBatch, m_pMaterial->GetOutlinePSO());
    }

    if (!nearly_equal(m_mtxPrevWorld, m_mtxWorld))
    {
        RenderBatch& velocityPassBatch = pRenderer->AddVelocityPassBatch();
        Draw(velocityPassBatch, m_pMaterial->GetVelocityPSO());
    }

    if (pRenderer->IsEnableMouseHitTest())
    {
        RenderBatch& idPassBatch = pRenderer->AddObjectIDPassBatch();
        Draw(idPassBatch, m_pMaterial->GetIDPSO());
    }
}

bool StaticMesh::FrustumCull(const float4* planes, uint32_t plane_count) const
{
    float3 center = mul(m_mtxWorld, float4(m_center, 1.0)).xyz();
    float radius = m_radius * max(max(abs(m_scale.x), abs(m_scale.y)), abs(m_scale.z));

    return ::FrustumCull(planes, plane_count, center, radius);
}

void StaticMesh::Draw(RenderBatch& batch, IGfxPipelineState* pso)
{
    uint32_t root_consts[1] = { m_nID };

    batch.label = m_name.c_str();
    batch.SetPipelineState(pso);
    batch.SetConstantBuffer(0, root_consts, sizeof(root_consts));
    batch.SetConstantBuffer(1, &m_modelCB, sizeof(ModelConstant));
    batch.SetConstantBuffer(2, m_pMaterial->GetConstants(), sizeof(MaterialConstant));
    batch.SetIndexBuffer(m_pRenderer->GetSceneBuffer(), m_indexBufferAddress, m_indexBufferFormat);
    batch.DrawIndexed(m_nIndexCount);
}

void StaticMesh::Dispatch(RenderBatch& batch, IGfxPipelineState* pso)
{
    uint32_t root_consts[5] = {
        m_nMeshletCount,
        m_meshletBufferAddress,
        1,
        m_meshletVerticesBufferAddress,
        m_meshletIndicesBufferAddress
    };

    batch.label = m_name.c_str();
    batch.SetPipelineState(pso);
    batch.SetConstantBuffer(0, root_consts, sizeof(root_consts));
    batch.SetConstantBuffer(1, &m_modelCB, sizeof(ModelConstant));
    batch.SetConstantBuffer(2, m_pMaterial->GetConstants(), sizeof(MaterialConstant));

    batch.DispatchMesh((m_nMeshletCount + 31 ) / 32, 1, 1);
}
