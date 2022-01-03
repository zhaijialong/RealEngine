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
    RenderFunc bassPassBatch = std::bind(&StaticMesh::RenderBassPass, this, std::placeholders::_1, std::placeholders::_2);
    pRenderer->AddGBufferPassBatch(bassPassBatch);

    if (m_nID == pRenderer->GetMouseHitObjectID())
    {
        RenderFunc outlinePassBatch = std::bind(&StaticMesh::RenderOutlinePass, this, std::placeholders::_1, std::placeholders::_2);
        pRenderer->AddForwardPassBatch(outlinePassBatch);
    }

    ShadowFunc shadowPassBatch = std::bind(&StaticMesh::RenderShadowPass, this, std::placeholders::_1, std::placeholders::_2);
    pRenderer->AddShadowPassBatch(shadowPassBatch);

    if (!nearly_equal(m_mtxPrevWorld, m_mtxWorld))
    {
        RenderFunc velocityPassBatch = std::bind(&StaticMesh::RenderVelocityPass, this, std::placeholders::_1, std::placeholders::_2);
        pRenderer->AddVelocityPassBatch(velocityPassBatch);
    }

    RenderFunc idPassBatch = std::bind(&StaticMesh::RenderIDPass, this, std::placeholders::_1, std::placeholders::_2);
    pRenderer->AddObjectIDPassBatch(idPassBatch);
}

bool StaticMesh::FrustumCull(const float4* planes, uint32_t plane_count) const
{
    float3 center = mul(m_mtxWorld, float4(m_center, 1.0)).xyz();
    float radius = m_radius * max(max(abs(m_scale.x), abs(m_scale.y)), abs(m_scale.z));

    return ::FrustumCull(planes, plane_count, center, radius);
}

void StaticMesh::RenderBassPass(IGfxCommandList* pCommandList, const Camera* pCamera)
{
    GPU_EVENT_DEBUG(pCommandList, m_name);
    //Draw(pCommandList, m_pMaterial->GetPSO());

    //AS is not good for small objects, lots of "DispatchMesh(1, 1, 1)" is 10x slower than VS
    //should implement some sort of Merging & Compact on GPU (aka. GPU-Driven rendering)
    Dispatch(pCommandList, m_pMaterial->GetMeshletPSO());
}

void StaticMesh::RenderOutlinePass(IGfxCommandList* pCommandList, const Camera* pCamera)
{
    GPU_EVENT_DEBUG(pCommandList, m_name);
    Draw(pCommandList, m_pMaterial->GetOutlinePSO());
}

void StaticMesh::RenderShadowPass(IGfxCommandList* pCommandList, const ILight* pLight)
{
    GPU_EVENT_DEBUG(pCommandList, m_name);
    Draw(pCommandList, m_pMaterial->GetShadowPSO());
}

void StaticMesh::RenderVelocityPass(IGfxCommandList* pCommandList, const Camera* pCamera)
{
    GPU_EVENT_DEBUG(pCommandList, m_name);
    Draw(pCommandList, m_pMaterial->GetVelocityPSO());
}

void StaticMesh::RenderIDPass(IGfxCommandList* pCommandList, const Camera* pCamera)
{
    GPU_EVENT_DEBUG(pCommandList, m_name);

    uint32_t root_consts[1] = { m_nID };
    pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));

    Draw(pCommandList, m_pMaterial->GetIDPSO());
}

void StaticMesh::Draw(IGfxCommandList* pCommandList, IGfxPipelineState* pso)
{
    pCommandList->SetPipelineState(pso);
    pCommandList->SetGraphicsConstants(1, &m_modelCB, sizeof(ModelConstant));
    pCommandList->SetGraphicsConstants(2, m_pMaterial->GetConstants(), sizeof(MaterialConstant));
    pCommandList->SetIndexBuffer(m_pRenderer->GetSceneBuffer(), m_indexBufferAddress, m_indexBufferFormat);
    pCommandList->DrawIndexed(m_nIndexCount);
}

void StaticMesh::Dispatch(IGfxCommandList* pCommandList, IGfxPipelineState* pso)
{
    uint32_t root_consts[5] = {
        m_nMeshletCount,
        m_meshletBufferAddress,
        1,
        m_meshletVerticesBufferAddress,
        m_meshletIndicesBufferAddress
    };

    pCommandList->SetPipelineState(pso);
    pCommandList->SetGraphicsConstants(0, root_consts, sizeof(root_consts));
    pCommandList->SetGraphicsConstants(1, &m_modelCB, sizeof(ModelConstant));
    pCommandList->SetGraphicsConstants(2, m_pMaterial->GetConstants(), sizeof(MaterialConstant));

    pCommandList->DispatchMesh((m_nMeshletCount + 31 ) / 32, 1, 1);
}
