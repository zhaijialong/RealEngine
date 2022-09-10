#pragma once

#include "visible_object.h"

class Skeleton;
class Animation;
class MeshMaterial;

struct SkeletalMeshData
{
    eastl::string name;
    uint32_t nodeID;

    eastl::unique_ptr<MeshMaterial> material;
    eastl::unique_ptr<IGfxRayTracingBLAS> blas;

    uint32_t uvBufferAddress = -1;
    uint32_t jointIDBufferAddress = -1;
    uint32_t jointWeightBufferAddress = -1;

    uint32_t staticPosBufferAddress = -1;
    uint32_t staticNormalBufferAddress = -1;
    uint32_t staticTangentBufferAddress = -1;

    uint32_t animPosBufferAddress = -1;
    uint32_t animNormalBufferAddress = -1;
    uint32_t animTangentBufferAddress = -1;

    uint32_t prevAnimPosBufferAddress = -1;

    uint32_t indexBufferAddress = -1;
    GfxFormat indexBufferFormat;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    InstanceData instanceData = {};
    uint32_t instanceIndex = 0;

    float3 center;
    float radius = 0.0;

    ~SkeletalMeshData();
};

struct SkeletalMeshNode
{
    eastl::string name;
    uint32_t id;
    uint32_t parent;
    eastl::vector<uint32_t> children;
    eastl::vector<eastl::unique_ptr<SkeletalMeshData>> meshes;

    //local transform
    float3 translation;
    float4 rotation;
    float3 scale;

    float4x4 globalTransform;
};

class SkeletalMesh : public IVisibleObject
{
    friend class GLTFLoader;

public:
    SkeletalMesh(const eastl::string& name);

    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;
    virtual void OnGui() override;

    SkeletalMeshNode* GetNode(uint32_t node_id) const;

private:
    void Create(SkeletalMeshData* mesh);

    void UpdateNodeTransform(SkeletalMeshNode* node);
    void UpdateMeshConstants(SkeletalMeshNode* node);

    void Draw(const SkeletalMeshData* mesh);
    void UpdateVertexSkinning(ComputeBatch& batch, const SkeletalMeshData* mesh);
    void Draw(RenderBatch& batch, const SkeletalMeshData* mesh, IGfxPipelineState* pso);

private:
    Renderer* m_pRenderer = nullptr;
    eastl::string m_name;

    float4x4 m_mtxWorld;

    eastl::unique_ptr<Skeleton> m_pSkeleton;
    eastl::unique_ptr<Animation> m_pAnimation;

    eastl::vector<eastl::unique_ptr<SkeletalMeshNode>> m_nodes;
    eastl::vector<uint32_t> m_rootNodes;

    float m_radius = 0.0f;
    float m_boundScaleFactor = 3.0f;
};