#include "mesh_material.h"
#include "resource_cache.h"
#include "core/engine.h"
#include "utils/gui_util.h"

MeshMaterial::~MeshMaterial()
{
    ResourceCache* cache = ResourceCache::GetInstance();
    cache->ReleaseTexture2D(m_pDiffuseTexture);
    cache->ReleaseTexture2D(m_pSpecularGlossinessTexture);
    cache->ReleaseTexture2D(m_pAlbedoTexture);
    cache->ReleaseTexture2D(m_pMetallicRoughnessTexture);
    cache->ReleaseTexture2D(m_pNormalTexture);
    cache->ReleaseTexture2D(m_pEmissiveTexture);
    cache->ReleaseTexture2D(m_pAOTexture);
    cache->ReleaseTexture2D(m_pAnisotropicTangentTexture);
    cache->ReleaseTexture2D(m_pSheenColorTexture);
    cache->ReleaseTexture2D(m_pSheenRoughnessTexture);
}

IGfxPipelineState* MeshMaterial::GetPSO()
{
    if (m_pPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if (m_bPbrMetallicRoughness) defines.push_back("PBR_METALLIC_ROUGHNESS=1");
        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pMetallicRoughnessTexture)
        {
            defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");

            if (m_pAOTexture == m_pMetallicRoughnessTexture)
            {
                defines.push_back("AO_METALLIC_ROUGHNESS_TEXTURE=1");
            }
        }

        if (m_bPbrSpecularGlossiness) defines.push_back("PBR_SPECULAR_GLOSSINESS=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_pSpecularGlossinessTexture) defines.push_back("SPECULAR_GLOSSINESS_TEXTURE=1");

        if (m_pNormalTexture)
        {
            defines.push_back("NORMAL_TEXTURE=1");

            if (m_pNormalTexture->GetTexture()->GetDesc().format == GfxFormat::BC5UNORM)
            {
                defines.push_back("RG_NORMAL_TEXTURE=1");
            }
        }

        if (m_pAnisotropicTangentTexture)
        {
            defines.push_back("ANISOTROPIC_TANGENT_TEXTURE=1");
        }

        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");
        if (m_pEmissiveTexture) defines.push_back("EMISSIVE_TEXTURE=1");
        if (m_pAOTexture) defines.push_back("AO_TEXTURE=1");
        if (m_bDoubleSided) defines.push_back("DOUBLE_SIDED=1");
        if (m_pSheenColorTexture) defines.push_back("SHEEN_COLOR_TEXTURE=1");
        if (m_pSheenRoughnessTexture) defines.push_back("SHEEN_ROUGHNESS_TEXTURE=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = m_bDoubleSided ? GfxCullMode::None : GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
        psoDesc.rt_format[1] = GfxFormat::RGBA8SRGB;
        psoDesc.rt_format[2] = GfxFormat::RGBA8UNORM;
        psoDesc.rt_format[3] = GfxFormat::R11G11B10F;
        psoDesc.rt_format[4] = GfxFormat::RGBA8UNORM;
        psoDesc.depthstencil_format = GfxFormat::D32F;

        m_pPSO = pRenderer->GetPipelineState(psoDesc, "model PSO");
    }
    return m_pPSO;
}

IGfxPipelineState* MeshMaterial::GetShadowPSO()
{
    if (m_pShadowPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model_shadow.hlsl", "vs_main", "vs_6_6", defines);
        if (m_pAlbedoTexture && m_bAlphaTest)
        {
            psoDesc.ps = pRenderer->GetShader("model_shadow.hlsl", "ps_main", "ps_6_6", defines);
        }
        psoDesc.rasterizer_state.cull_mode = m_bDoubleSided ? GfxCullMode::None : GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.rasterizer_state.depth_bias = 5.0f;
        psoDesc.rasterizer_state.depth_slope_scale = 1.0f;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::LessEqual;
        psoDesc.depthstencil_format = GfxFormat::D16;

        m_pShadowPSO = pRenderer->GetPipelineState(psoDesc, "model shadow PSO");
    }
    return m_pShadowPSO;
}

IGfxPipelineState* MeshMaterial::GetVelocityPSO()
{
    if (m_pVelocityPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if (m_bSkeletalAnim) defines.push_back("ANIME_POS=1");
        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model_velocity.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model_velocity.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = m_bDoubleSided ? GfxCullMode::None : GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.depthstencil_state.depth_write = false;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::RG16F;
        psoDesc.depthstencil_format = GfxFormat::D32F;

        m_pVelocityPSO = pRenderer->GetPipelineState(psoDesc, "model velocity PSO");
    }
    return m_pVelocityPSO;
}

IGfxPipelineState* MeshMaterial::GetIDPSO()
{
    if (m_pIDPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model_id.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model_id.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = m_bDoubleSided ? GfxCullMode::None : GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.depthstencil_state.depth_write = false;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::R32UI;
        psoDesc.depthstencil_format = GfxFormat::D32F;

        m_pIDPSO = pRenderer->GetPipelineState(psoDesc, "model ID PSO");
    }
    return m_pIDPSO;
}

IGfxPipelineState* MeshMaterial::GetOutlinePSO()
{
    if (m_pOutlinePSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model_outline.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model_outline.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = GfxCullMode::Front;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.depthstencil_state.depth_write = false;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::RGBA16F;
        psoDesc.depthstencil_format = GfxFormat::D32F;

        m_pOutlinePSO = pRenderer->GetPipelineState(psoDesc, "model outline PSO");
    }
    return m_pOutlinePSO;
}

IGfxPipelineState* MeshMaterial::GetMeshletPSO()
{
    if (m_pMeshletPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;

        if (m_bPbrMetallicRoughness) defines.push_back("PBR_METALLIC_ROUGHNESS=1");
        if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (m_pMetallicRoughnessTexture)
        {
            defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");

            if (m_pAOTexture == m_pMetallicRoughnessTexture)
            {
                defines.push_back("AO_METALLIC_ROUGHNESS_TEXTURE=1");
            }
        }
        
        if (m_bPbrSpecularGlossiness) defines.push_back("PBR_SPECULAR_GLOSSINESS=1");
        if (m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if (m_pSpecularGlossinessTexture) defines.push_back("SPECULAR_GLOSSINESS_TEXTURE=1");

        if (m_pNormalTexture)
        {
            defines.push_back("NORMAL_TEXTURE=1");

            if (m_pNormalTexture->GetTexture()->GetDesc().format == GfxFormat::BC5UNORM)
            {
                defines.push_back("RG_NORMAL_TEXTURE=1");
            }
        }

        if (m_pAnisotropicTangentTexture)
        {
            defines.push_back("ANISOTROPIC_TANGENT_TEXTURE=1");
        }

        if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");
        if (m_pEmissiveTexture) defines.push_back("EMISSIVE_TEXTURE=1");
        if (m_pAOTexture) defines.push_back("AO_TEXTURE=1");
        if (m_bDoubleSided) defines.push_back("DOUBLE_SIDED=1");
        if (m_pSheenColorTexture) defines.push_back("SHEEN_COLOR_TEXTURE=1");
        if (m_pSheenRoughnessTexture) defines.push_back("SHEEN_ROUGHNESS_TEXTURE=1");

        GfxMeshShadingPipelineDesc psoDesc;
        psoDesc.as = pRenderer->GetShader("meshlet_culling.hlsl", "main_as", "as_6_6", defines);
        psoDesc.ms = pRenderer->GetShader("model_meshlet.hlsl", "main_ms", "ms_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = m_bDoubleSided ? GfxCullMode::None : GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = m_bFrontFaceCCW;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
        psoDesc.rt_format[1] = GfxFormat::RGBA8SRGB;
        psoDesc.rt_format[2] = GfxFormat::RGBA8UNORM;
        psoDesc.rt_format[3] = GfxFormat::R11G11B10F;
        psoDesc.rt_format[4] = GfxFormat::RGBA8UNORM;
        psoDesc.depthstencil_format = GfxFormat::D32F;

        m_pMeshletPSO = pRenderer->GetPipelineState(psoDesc, "model meshlet PSO");
    }
    return m_pMeshletPSO;
}

IGfxPipelineState* MeshMaterial::GetVertexSkinningPSO()
{
    if (m_pVertexSkinningPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        GfxComputePipelineDesc desc;
        desc.cs = pRenderer->GetShader("vertex_skinning.hlsl", "main", "cs_6_6", {});
        m_pVertexSkinningPSO = pRenderer->GetPipelineState(desc, "vertex skinning PSO");
    }
    return m_pVertexSkinningPSO;
}

void MeshMaterial::UpdateConstants()
{
    m_materialCB.shadingModel = (uint)m_shadingModel;
    m_materialCB.albedoTexture = m_pAlbedoTexture ? m_pAlbedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.metallicRoughnessTexture = m_pMetallicRoughnessTexture ? m_pMetallicRoughnessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.normalTexture = m_pNormalTexture ? m_pNormalTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.emissiveTexture = m_pEmissiveTexture ? m_pEmissiveTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.aoTexture = m_pAOTexture ? m_pAOTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.diffuseTexture = m_pDiffuseTexture ? m_pDiffuseTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.specularGlossinessTexture = m_pSpecularGlossinessTexture ? m_pSpecularGlossinessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.albedo = m_albedoColor;
    m_materialCB.emissive = m_emissiveColor;
    m_materialCB.metallic = m_metallic;
    m_materialCB.roughness = m_roughness;
    m_materialCB.alphaCutoff = m_alphaCutoff;
    m_materialCB.diffuse = m_diffuseColor;
    m_materialCB.specular = m_specularColor;
    m_materialCB.glossiness = m_glossiness;
    m_materialCB.anisotropy = m_anisotropy;
    m_materialCB.anisotropyTexture = m_pAnisotropicTangentTexture ? m_pAnisotropicTangentTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.sheenColorTexture = m_pSheenColorTexture ? m_pSheenColorTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.sheenRoughnessTexture = m_pSheenRoughnessTexture ? m_pSheenRoughnessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.sheenColor = m_sheenColor;
    m_materialCB.sheenRoughness = m_sheenRoughness;

    m_materialCB.bPbrMetallicRoughness = m_bPbrMetallicRoughness;
    m_materialCB.bPbrSpecularGlossiness = m_bPbrSpecularGlossiness;
    m_materialCB.bRGNormalTexture = m_pNormalTexture && (m_pNormalTexture->GetTexture()->GetDesc().format == GfxFormat::BC5UNORM);
    m_materialCB.bDoubleSided = m_bDoubleSided;
}

void MeshMaterial::OnGui()
{
    GUI("Inspector", "Material", [&]()
        {
            ImGui::Combo("Shading Model##Material", (int*)&m_shadingModel, "Default\0Anisotropy\0Sheen\0\0", (int)ShadingModel::Max);
            ImGui::SliderFloat("Metallic##Material", &m_metallic, .0f, 1.0f);
            ImGui::SliderFloat("Roughness##Material", &m_roughness, .0f, 1.0f);

            switch (m_shadingModel)
            {
            case ShadingModel::Anisotropy:
                ImGui::SliderFloat("Anisotropy##Material", &m_anisotropy, -1.0f, 1.0f);
                break;
            default:
                break;
            }
        });
}
