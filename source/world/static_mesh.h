#pragma once

#include "i_visible_object.h"
#include "model_constants.hlsli"

class MeshMaterial;

class StaticMesh : public IVisibleObject
{
    friend class GLTFLoader;
public:
    StaticMesh(const std::string& name);

    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;

private:
    void RenderBassPass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderOutlinePass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderShadowPass(IGfxCommandList* pCommandList, const ILight* pLight);
    void RenderVelocityPass(IGfxCommandList* pCommandList, const Camera* pCamera);
    void RenderIDPass(IGfxCommandList* pCommandList, const Camera* pCamera);

    void UpdateConstants();
    void Draw(IGfxCommandList* pCommandList, IGfxPipelineState* pso);

private:
    std::string m_name;
    std::unique_ptr<MeshMaterial> m_pMaterial = nullptr;

    std::unique_ptr<IndexBuffer> m_pIndexBuffer;
    std::unique_ptr<StructuredBuffer> m_pPosBuffer;
    std::unique_ptr<StructuredBuffer> m_pUVBuffer;
    std::unique_ptr<StructuredBuffer> m_pNormalBuffer;
    std::unique_ptr<StructuredBuffer> m_pTangentBuffer;

    ModelConstant m_modelCB = {};

    float4x4 m_mtxWorld;
    float4x4 m_mtxPrevWorld;
};