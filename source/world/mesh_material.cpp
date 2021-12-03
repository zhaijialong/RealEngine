#include "mesh_material.h"
#include "core/engine.h"

IGfxPipelineState* MeshMaterial::GetPSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_pMetallicRoughnessTexture)
    {
        defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");

        if (m_pAOTexture == m_pMetallicRoughnessTexture)
        {
            defines.push_back("AO_METALLIC_ROUGHNESS_TEXTURE=1");
        }
    }

    if (m_pNormalTexture)
    {
        defines.push_back("NORMAL_TEXTURE=1");

        if (m_pNormalTexture->GetTexture()->GetDesc().format == GfxFormat::BC5UNORM)
        {
            defines.push_back("RG_NORMAL_TEXTURE=1");
        }
    }

    if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");
    if (m_pEmissiveTexture) defines.push_back("EMISSIVE_TEXTURE=1");
    if (m_pAOTexture) defines.push_back("AO_TEXTURE=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
    psoDesc.rt_format[1] = GfxFormat::RGBA8UNORM;
    psoDesc.rt_format[2] = GfxFormat::RGBA8SRGB;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model PSO");
}

IGfxPipelineState* MeshMaterial::GetShadowPSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_shadow.hlsl", "vs_main", "vs_6_6", defines);
    if (m_pAlbedoTexture && m_bAlphaTest)
    {
        psoDesc.ps = pRenderer->GetShader("model_shadow.hlsl", "ps_main", "ps_6_6", defines);
    }
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.rasterizer_state.depth_bias = 5.0f;
    psoDesc.rasterizer_state.depth_slope_scale = 1.0f;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::LessEqual;
    psoDesc.depthstencil_format = GfxFormat::D16;

    return pRenderer->GetPipelineState(psoDesc, "model shadow PSO");
}

IGfxPipelineState* MeshMaterial::GetVelocityPSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_bSkeletalAnim) defines.push_back("ANIME_POS=1");
    if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_velocity.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model_velocity.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA16F;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model velocity PSO");
}

IGfxPipelineState* MeshMaterial::GetIDPSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_id.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model_id.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::R32UI;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model ID PSO");
}

IGfxPipelineState* MeshMaterial::GetOutlinePSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_outline.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model_outline.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Front;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_write = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA16F;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model outline PSO");
}

IGfxPipelineState* MeshMaterial::GetMeshletPSO()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    GfxMeshShadingPipelineDesc psoDesc;
    psoDesc.ms = pRenderer->GetShader("meshlet.hlsl", "main_ms", "ms_6_6", {});
    psoDesc.ps = pRenderer->GetShader("meshlet.hlsl", "main_ps", "ps_6_6", {});
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
    psoDesc.rt_format[1] = GfxFormat::RGBA8UNORM;
    psoDesc.rt_format[2] = GfxFormat::RGBA8SRGB;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model meshlet PSO");
}

void MeshMaterial::UpdateConstants()
{
    m_materialCB.albedoTexture = m_pAlbedoTexture ? m_pAlbedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.metallicRoughnessTexture = m_pMetallicRoughnessTexture ? m_pMetallicRoughnessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.normalTexture = m_pNormalTexture ? m_pNormalTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.emissiveTexture = m_pEmissiveTexture ? m_pEmissiveTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.aoTexture = m_pAOTexture ? m_pAOTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    m_materialCB.albedo = m_albedoColor;
    m_materialCB.emissive = m_emissiveColor;
    m_materialCB.metallic = m_metallic;
    m_materialCB.roughness = m_roughness;
    m_materialCB.alphaCutoff = m_alphaCutoff;
}
