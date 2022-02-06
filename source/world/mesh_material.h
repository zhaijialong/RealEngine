#pragma once

#include "renderer/renderer.h"
#include "model_constants.hlsli"

class MeshMaterial
{
    friend class GLTFLoader;
public:
    ~MeshMaterial();

    IGfxPipelineState* GetPSO();
    IGfxPipelineState* GetShadowPSO();
    IGfxPipelineState* GetVelocityPSO();
    IGfxPipelineState* GetIDPSO();
    IGfxPipelineState* GetOutlinePSO();

    IGfxPipelineState* GetMeshletPSO();

    IGfxPipelineState* GetVertexSkinningPSO();

    void UpdateConstants();
    const ModelMaterialConstant* GetConstants() const { return &m_materialCB; }

    bool IsFrontFaceCCW() const { return m_bFrontFaceCCW; }
    bool IsAlphaTest() const { return m_bAlphaTest; }
    bool IsAlphaBlend() const { return m_bAlphaBlend; }
    bool IsVertexSkinned() const { return m_bSkeletalAnim; }

private:
    std::string m_name;
    ModelMaterialConstant m_materialCB = {};

    IGfxPipelineState* m_pPSO = nullptr;
    IGfxPipelineState* m_pShadowPSO = nullptr;
    IGfxPipelineState* m_pVelocityPSO = nullptr;
    IGfxPipelineState* m_pIDPSO = nullptr;
    IGfxPipelineState* m_pOutlinePSO = nullptr;

    IGfxPipelineState* m_pMeshletPSO = nullptr;

    IGfxPipelineState* m_pVertexSkinningPSO = nullptr;

    //pbr_specular_glossiness
    Texture2D* m_pDiffuseTexture = nullptr;
    Texture2D* m_pSpecularGlossinessTexture = nullptr;
    float3 m_diffuseColor = float3(1.0f, 1.0f, 1.0f);
    float3 m_specularColor = float3(0.0f, 0.0f, 0.0f);
    float m_glossiness = 0.0f;

    //pbr_metallic_roughness
    Texture2D* m_pAlbedoTexture = nullptr;
    Texture2D* m_pMetallicRoughnessTexture = nullptr;
    float3 m_albedoColor = float3(1.0f, 1.0f, 1.0f);
    float m_metallic = 0.0f;
    float m_roughness = 0.0f;

    Texture2D* m_pNormalTexture = nullptr;
    Texture2D* m_pEmissiveTexture = nullptr;
    Texture2D* m_pAOTexture = nullptr;
    float3 m_emissiveColor = float3(0.0f, 0.0f, 0.0f);
    float m_alphaCutoff = 0.0f;
    bool m_bAlphaTest = false;

    bool m_bAlphaBlend = false;
    bool m_bSkeletalAnim = false;
    bool m_bFrontFaceCCW = true;
    bool m_bDoubleSided = false;
    bool m_bPbrSpecularGlossiness = false;
    bool m_bPbrMetallicRoughness = false;
};