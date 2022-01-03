#pragma once

#include "i_visible_object.h"
#include "model_constants.hlsli"

class MeshMaterial;
class Renderer;

class StaticMesh : public IVisibleObject
{
    friend class GLTFLoader;
public:
    StaticMesh(const std::string& name);
    ~StaticMesh();

    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;

private:
    void RenderBassPass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderOutlinePass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderShadowPass(IGfxCommandList* pCommandList, const ILight* pLight);
    void RenderVelocityPass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderIDPass(IGfxCommandList* pCommandList, const Camera* pCamera);

    void UpdateConstants();
    void Draw(IGfxCommandList* pCommandList, IGfxPipelineState* pso);
    void Dispatch(IGfxCommandList* pCommandList, IGfxPipelineState* pso);

private:
    Renderer* m_pRenderer = nullptr;
    std::string m_name;
    std::unique_ptr<MeshMaterial> m_pMaterial = nullptr;

    uint32_t m_posBufferAddress = -1;
    uint32_t m_uvBufferAddress = -1;
    uint32_t m_normalBufferAddress = -1;
    uint32_t m_tangentBufferAddress = -1;

    uint32_t m_meshletBufferAddress = -1;
    uint32_t m_meshletVerticesBufferAddress = -1;
    uint32_t m_meshletIndicesBufferAddress = -1;

    uint32_t m_indexBufferAddress = -1;
    GfxFormat m_indexBufferFormat;
    uint32_t m_nIndexCount = 0;

    uint32_t m_nMeshletCount = 0;
    ModelConstant m_modelCB = {};

    float4x4 m_mtxWorld;
    float4x4 m_mtxPrevWorld;

    float3 m_center = { 0.0f, 0.0f, 0.0f };
    float m_radius = 0.0f;
};