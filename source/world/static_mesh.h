#pragma once

#include "renderer/render_batch.h"
#include "visible_object.h"
#include "model_constants.hlsli"

class MeshMaterial;
class IPhysicsShape;
class IPhysicsRigidBody;

class StaticMesh : public IVisibleObject
{
    friend class GLTFLoader;
public:
    StaticMesh(const eastl::string& name);
    ~StaticMesh();

    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;
    virtual void OnGui() override;

    virtual void SetPosition(const float3& pos) override;
    virtual void SetRotation(const quaternion& rotation) override;
    virtual void SetScale(const float3& scale) override;

    IPhysicsRigidBody* GetPhysicsBody() const { return m_pRigidBody.get(); }
    void SetPhysicsBody(IPhysicsRigidBody* body);

    MeshMaterial* GetMaterial() const { return m_pMaterial.get(); }

private:
    void UpdateConstants();
    void Draw(RenderBatch& batch, IGfxPipelineState* pso);
    void Dispatch(RenderBatch& batch, IGfxPipelineState* pso);

private:
    Renderer* m_pRenderer = nullptr;
    eastl::string m_name;
    eastl::unique_ptr<MeshMaterial> m_pMaterial = nullptr;
    eastl::unique_ptr<IGfxRayTracingBLAS> m_pBLAS;
    eastl::unique_ptr<IPhysicsRigidBody> m_pRigidBody;
    eastl::unique_ptr<IPhysicsShape> m_pShape;

    uint32_t m_posBufferAddress = -1;
    uint32_t m_uvBufferAddress = -1;
    uint32_t m_normalBufferAddress = -1;
    uint32_t m_tangentBufferAddress = -1;

    uint32_t m_meshletBufferAddress = -1;
    uint32_t m_meshletVerticesBufferAddress = -1;
    uint32_t m_meshletIndicesBufferAddress = -1;
    uint32_t m_nMeshletCount = 0;

    uint32_t m_indexBufferAddress = -1;
    GfxFormat m_indexBufferFormat;
    uint32_t m_nIndexCount = 0;
    uint32_t m_nVertexCount = 0;

    InstanceData m_instanceData = {};
    uint32_t m_nInstanceIndex = 0;

    float3 m_center = { 0.0f, 0.0f, 0.0f };
    float m_radius = 0.0f;

    bool m_bShowBoundingSphere = false;
    bool m_bShowTangent = false;
    bool m_bShowBitangent = false;
    bool m_bShowNormal = false;
};