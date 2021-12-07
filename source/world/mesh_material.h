#pragma once

#include "renderer/renderer.h"
#include "model_constants.hlsli"

class MeshMaterial
{
    friend class GLTFLoader;
public:
    IGfxPipelineState* GetPSO();
    IGfxPipelineState* GetShadowPSO();
    IGfxPipelineState* GetVelocityPSO();
    IGfxPipelineState* GetIDPSO();
    IGfxPipelineState* GetOutlinePSO();

    IGfxPipelineState* GetMeshletPSO();

    void UpdateConstants();
    const MaterialConstant* GetConstants() const { return &m_materialCB; }

private:
    std::string m_name;
    MaterialConstant m_materialCB = {};

    IGfxPipelineState* m_pPSO = nullptr;
    IGfxPipelineState* m_pShadowPSO = nullptr;
    IGfxPipelineState* m_pVelocityPSO = nullptr;
    IGfxPipelineState* m_pIDPSO = nullptr;
    IGfxPipelineState* m_pOutlinePSO = nullptr;

    IGfxPipelineState* m_pMeshletPSO = nullptr;

    Texture2D* m_pAlbedoTexture = nullptr;
    Texture2D* m_pMetallicRoughnessTexture = nullptr;
    Texture2D* m_pNormalTexture = nullptr;
    Texture2D* m_pEmissiveTexture = nullptr;
    Texture2D* m_pAOTexture = nullptr;

    float3 m_albedoColor = float3(1.0f, 1.0f, 1.0f);
    float3 m_emissiveColor = float3(0.0f, 0.0f, 0.0f);
    float m_metallic = 0.0f;
    float m_roughness = 0.0f;
    float m_alphaCutoff = 0.0f;
    bool m_bAlphaTest = false;

    bool m_bSkeletalAnim = false;
};