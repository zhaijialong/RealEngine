#include "skeletal_mesh.h"
#include "skeleton.h"
#include "animation.h"
#include "mesh_material.h"
#include "resource_cache.h"
#include "core/engine.h"

SkeletalMeshData::~SkeletalMeshData()
{
    ResourceCache* cache = ResourceCache::GetInstance();

    cache->RelaseSceneBuffer(uvBufferAddress);
    cache->RelaseSceneBuffer(jointIDBufferAddress);
    cache->RelaseSceneBuffer(jointWeightBufferAddress);

    cache->RelaseSceneBuffer(staticPosBufferAddress);
    cache->RelaseSceneBuffer(staticNormalBufferAddress);
    cache->RelaseSceneBuffer(staticTangentBufferAddress);

    cache->RelaseSceneBuffer(indexBufferAddress);

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    pRenderer->FreeSceneAnimationBuffer(animPosBufferAddress);
    pRenderer->FreeSceneAnimationBuffer(animNormalBufferAddress);
    pRenderer->FreeSceneAnimationBuffer(animTangentBufferAddress);

    pRenderer->FreeSceneAnimationBuffer(prevAnimPosBufferAddress);
}

SkeletalMesh::SkeletalMesh(const std::string& name)
{
    m_name = name;
}

SkeletalMesh::~SkeletalMesh()
{
}

bool SkeletalMesh::Create()
{
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        for (size_t j = 0; j < m_nodes[i]->meshes.size(); ++j)
        {
            SkeletalMeshData* mesh = m_nodes[i]->meshes[j].get();

            Create(mesh);
        }
    }

    return true;
}

void SkeletalMesh::Tick(float delta_time)
{
    m_pAnimation->Update(this, delta_time);

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 S = scaling_matrix(m_scale);
    m_mtxWorld = mul(T, mul(R, S));

    for (size_t i = 0; i < m_rootNodes.size(); ++i)
    {
        UpdateMeshNode(GetNode(m_rootNodes[i]));
    }

    if (m_pSkeleton)
    {
        m_pSkeleton->Update(this);
    }
}

void SkeletalMesh::Render(Renderer* pRenderer)
{
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        for (size_t j = 0; j < m_nodes[i]->meshes.size(); ++j)
        {
            const SkeletalMeshData* mesh = m_nodes[i]->meshes[j].get();
            Draw(mesh);
        }
    }
}

bool SkeletalMesh::FrustumCull(const float4* planes, uint32_t plane_count) const
{
    return true; //todo
}

SkeletalMeshNode* SkeletalMesh::GetNode(uint32_t node_id) const
{
    RE_ASSERT(node_id < m_nodes.size());
    return m_nodes[node_id].get();
}

void SkeletalMesh::UpdateMeshNode(SkeletalMeshNode* node)
{
    float4x4 T = translation_matrix(node->translation);
    float4x4 R = rotation_matrix(node->rotation);
    float4x4 S = scaling_matrix(node->scale);
    float4x4 globalTransform = mul(T, mul(R, S));

    if (node->parent != -1)
    {
        globalTransform = mul(m_nodes[node->parent]->globalTransform, globalTransform);
    }

    node->globalTransform = globalTransform;

    for (size_t i = 0; i < node->meshes.size(); ++i)
    {
        UpdateMeshData(node->meshes[i].get());
    }

    for (size_t i = 0; i < node->children.size(); ++i)
    {
        UpdateMeshNode(GetNode(node->children[i]));
    }
}

void SkeletalMesh::UpdateMeshData(SkeletalMeshData* mesh)
{
    std::swap(mesh->prevAnimPosBufferAddress, mesh->animPosBufferAddress);

    mesh->material->UpdateConstants();

    mesh->instanceData.instanceType = (uint)InstanceType::Model;
    mesh->instanceData.indexBufferAddress = mesh->indexBufferAddress;
    mesh->instanceData.indexStride = mesh->indexBufferFormat == GfxFormat::R32UI ? 4 : 2;
    mesh->instanceData.triangleCount = mesh->indexCount / 3;

    mesh->instanceData.uvBufferAddress = mesh->uvBufferAddress;

    bool isSkinnedMesh = mesh->material->IsVertexSkinned();
    if (isSkinnedMesh)
    {
        mesh->instanceData.posBufferAddress = mesh->animPosBufferAddress;
        mesh->instanceData.normalBufferAddress = mesh->animNormalBufferAddress;
        mesh->instanceData.tangentBufferAddress = mesh->animTangentBufferAddress;
    }
    else
    {
        mesh->instanceData.posBufferAddress = mesh->staticPosBufferAddress;
        mesh->instanceData.normalBufferAddress = mesh->staticNormalBufferAddress;
        mesh->instanceData.tangentBufferAddress = mesh->staticTangentBufferAddress;
    }

    mesh->instanceData.bVertexAnimation = isSkinnedMesh;
    mesh->instanceData.materialDataAddress = m_pRenderer->AllocateSceneConstant((void*)mesh->material->GetConstants(), sizeof(ModelMaterialConstant));
    mesh->instanceData.objectID = m_nID;
    mesh->instanceData.scale = max(max(abs(m_scale.x), abs(m_scale.y)), abs(m_scale.z));

    //todo
    //mesh->instanceData.center = mul(mtxWorld, float4(m_center, 1.0)).xyz();
    //mesh->instanceData.radius = m_radius * mesh->instanceData.scale;

    mesh->instanceData.mtxPrevWorld = mesh->instanceData.mtxWorld;
    mesh->instanceData.mtxWorld = isSkinnedMesh ? m_mtxWorld : mul(m_mtxWorld, m_nodes[mesh->nodeID]->globalTransform);
    mesh->instanceData.mtxWorldInverseTranspose = transpose(inverse(mesh->instanceData.mtxWorld));

    GfxRayTracingInstanceFlag flags = mesh->material->IsFrontFaceCCW() ? GfxRayTracingInstanceFlagFrontFaceCCW : 0;
    mesh->instanceIndex = m_pRenderer->AddInstance(mesh->instanceData, mesh->blas.get(), flags);
}

void SkeletalMesh::Create(SkeletalMeshData* mesh)
{
    if (mesh->material->IsVertexSkinned())
    {
        mesh->animPosBufferAddress = m_pRenderer->AllocateSceneAnimationBuffer(sizeof(float3) * mesh->vertexCount);
        mesh->prevAnimPosBufferAddress = m_pRenderer->AllocateSceneAnimationBuffer(sizeof(float3) * mesh->vertexCount);

        if (mesh->staticNormalBufferAddress != -1)
        {
            mesh->animNormalBufferAddress = m_pRenderer->AllocateSceneAnimationBuffer(sizeof(float3) * mesh->vertexCount);
        }

        if (mesh->staticTangentBufferAddress != -1)
        {
            mesh->animTangentBufferAddress = m_pRenderer->AllocateSceneAnimationBuffer(sizeof(float4) * mesh->vertexCount);
        }
    }

    GfxRayTracingGeometry geometry;
    if(mesh->material->IsVertexSkinned())
    {
        geometry.vertex_buffer = m_pRenderer->GetSceneAnimationBuffer();
        geometry.vertex_buffer_offset = mesh->prevAnimPosBufferAddress;
    }
    else
    {
        geometry.vertex_buffer = m_pRenderer->GetSceneStaticBuffer();
        geometry.vertex_buffer_offset = mesh->staticPosBufferAddress;
    }
    geometry.vertex_count = mesh->vertexCount;
    geometry.vertex_stride = sizeof(float3);
    geometry.vertex_format = GfxFormat::RGB32F;
    geometry.index_buffer = m_pRenderer->GetSceneStaticBuffer();
    geometry.index_buffer_offset = mesh->indexBufferAddress;
    geometry.index_count = mesh->indexCount;
    geometry.index_format = mesh->indexBufferFormat;
    geometry.opaque = mesh->material->IsAlphaTest() ? false : true; //todo : alpha blend

    GfxRayTracingBLASDesc desc;
    desc.geometries.push_back(geometry);
    desc.flags = GfxRayTracingASFlagAllowCompaction | GfxRayTracingASFlagPreferFastTrace; //todo : update

    IGfxDevice* device = m_pRenderer->GetDevice();
    mesh->blas.reset(device->CreateRayTracingBLAS(desc, "BLAS : " + m_name));
    m_pRenderer->BuildRayTracingBLAS(mesh->blas.get());
}

void SkeletalMesh::Draw(const SkeletalMeshData* mesh)
{
    if (mesh->material->IsAlphaBlend())
    {
        return; //todo
    }

    if (mesh->material->IsVertexSkinned())
    {
        ComputeBatch& batch = m_pRenderer->AddAnimationBatch();
        UpdateVertexSkinning(batch, mesh);
    }

    RenderBatch& batch = m_pRenderer->AddBasePassBatch();
    Draw(batch, mesh, mesh->material->GetPSO());

    if (mesh->material->IsVertexSkinned() || !nearly_equal(mesh->instanceData.mtxPrevWorld, mesh->instanceData.mtxWorld))
    {
        RenderBatch& velocityPassBatch = m_pRenderer->AddVelocityPassBatch();
        Draw(velocityPassBatch, mesh, mesh->material->GetVelocityPSO());
    }

    if (m_pRenderer->IsEnableMouseHitTest())
    {
        RenderBatch& idPassBatch = m_pRenderer->AddObjectIDPassBatch();
        Draw(idPassBatch, mesh, mesh->material->GetIDPSO());
    }

    if (m_nID == m_pRenderer->GetMouseHitObjectID())
    {
        RenderBatch& outlinePassBatch = m_pRenderer->AddForwardPassBatch();
        Draw(outlinePassBatch, mesh, mesh->material->GetOutlinePSO());
    }
}

void SkeletalMesh::UpdateVertexSkinning(ComputeBatch& batch, const SkeletalMeshData* mesh)
{
    batch.label = m_name.c_str();
    batch.SetPipelineState(mesh->material->GetVertexSkinningPSO());

    uint32_t cb[10] = {
        mesh->vertexCount,

        mesh->staticPosBufferAddress,
        mesh->staticNormalBufferAddress,
        mesh->staticTangentBufferAddress,

        mesh->animPosBufferAddress,
        mesh->animNormalBufferAddress,
        mesh->animTangentBufferAddress,

        mesh->jointIDBufferAddress,
        mesh->jointWeightBufferAddress,
        m_pSkeleton->GetJointMatricesAddress(),
    };

    batch.SetConstantBuffer(1, cb, sizeof(cb));
    batch.Dispatch((mesh->vertexCount + 63) / 64, 1, 1);
}

void SkeletalMesh::Draw(RenderBatch& batch, const SkeletalMeshData* mesh, IGfxPipelineState* pso)
{
    uint32_t root_consts[2] = { mesh->instanceIndex, mesh->prevAnimPosBufferAddress };

    batch.label = m_name.c_str();
    batch.SetPipelineState(pso);
    batch.SetConstantBuffer(0, root_consts, sizeof(root_consts));

    batch.SetIndexBuffer(m_pRenderer->GetSceneStaticBuffer(), mesh->indexBufferAddress, mesh->indexBufferFormat);
    batch.DrawIndexed(mesh->indexCount);
}
