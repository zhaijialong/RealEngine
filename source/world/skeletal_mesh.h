#pragma once

#include "i_visible_object.h"

class Skeleton;
class Animation;
class MeshMaterial;

struct SkeletalMeshData
{
    std::string name;
    uint32_t nodeID;

    std::unique_ptr<MeshMaterial> material;

    uint32_t uvBufferAddress = -1;
    uint32_t jointIDBufferAddress = -1;
    uint32_t jointWeightBufferAddress = -1;

    uint32_t staticPosBufferAddress = -1;
    uint32_t staticNormalBufferAddress = -1;
    uint32_t staticTangentBufferAddress = -1;

    uint32_t animPosBufferAddress = -1;
    uint32_t animNormalBufferAddress = -1;
    uint32_t animTangentBufferAddress = -1;

    uint32_t indexBufferAddress = -1;
    GfxFormat indexBufferFormat;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    InstanceData instanceData = {};
    uint32_t instanceIndex = 0;

    ~SkeletalMeshData();
};

struct SkeletalMeshNode
{
    std::string name;
    uint32_t id;
    uint32_t parent;
    std::vector<uint32_t> children;
    std::vector<std::unique_ptr<SkeletalMeshData>> meshes;

    float3 translation;
    float4 rotation;
    float3 scale;

    float4x4 globalTransform;
};

class SkeletalMesh : public IVisibleObject
{
    friend class GLTFLoader;

public:
    SkeletalMesh(const std::string& name);
    ~SkeletalMesh();

    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;

    SkeletalMeshNode* GetNode(uint32_t node_id) const;

private:
    void UpdateMeshNode(SkeletalMeshNode* node);
    void UpdateMeshData(SkeletalMeshData* mesh);

    void Draw(const SkeletalMeshData* mesh);

private:
    Renderer* m_pRenderer = nullptr;
    std::string m_name;

    float4x4 m_mtxWorld;

    std::unique_ptr<Skeleton> m_pSkeleton;
    std::unique_ptr<Animation> m_pAnimation;

    std::vector<std::unique_ptr<SkeletalMeshNode>> m_nodes;
    std::vector<uint32_t> m_rootNodes;
};