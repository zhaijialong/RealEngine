#include "renderer.h"
#include "core/engine.h"
#include "global_constants.hlsli"


//test code
inline float4x4 GetLightVP(ILight* light)
{
    float3 light_dir = light->GetLightDirection();
    float3 eye = light_dir * 100.0f;
    float4x4 mtxView = lookat_matrix(eye, float3(0, 0, 0), float3(0, 1, 0), linalg::pos_z);
    float4x4 mtxProj = ortho_matrix(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 500.0f);
    float4x4 mtxVP = mul(mtxProj, mtxView);
    return mtxVP;
}

RenderGraphHandle Renderer::BuildRenderGraph()
{
    struct DepthPassData
    {
        RenderGraphHandle depthRT;
    };

    auto shadow_pass = m_pRenderGraph->AddPass<DepthPassData>("Shadow Pass",
        [](DepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = desc.height = 4096;
            desc.format = GfxFormat::D16;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;

            data.depthRT = builder.Create<RenderGraphTexture>(desc, "ShadowMap");
            data.depthRT = builder.WriteDepth(data.depthRT, 0, GfxRenderPassLoadOp::Clear, 1.0f);
        },
        [&](const DepthPassData& data, IGfxCommandList* pCommandList)
        {
            ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
            float4x4 mtxVP = GetLightVP(light);

            for (size_t i = 0; i < m_shadowPassBatchs.size(); ++i)
            {
                m_shadowPassBatchs[i](pCommandList, this, mtxVP);
            }
            m_shadowPassBatchs.clear();
        });

    struct GBufferPassData
    {
        RenderGraphHandle albedoRT; //srgb : albedo(xyz) + AO(a) 
        RenderGraphHandle normalRT; //rgb10a2 : normal(xy) + roughness(z)
        RenderGraphHandle emissiveRT; //srgb : emissive(xyz) + metalness(a)
        RenderGraphHandle depthRT;
    };

    auto gbuffer_pass = m_pRenderGraph->AddPass<GBufferPassData>("GBuffer Pass",
        [&](GBufferPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            desc.format = GfxFormat::RGBA8SRGB;
            data.albedoRT = builder.Create<RenderGraphTexture>(desc, "Albedo RT");
            data.emissiveRT = builder.Create<RenderGraphTexture>(desc, "Emissive RT");

            desc.format = GfxFormat::RGB10A2UNORM;
            data.normalRT = builder.Create<RenderGraphTexture>(desc, "Normal RT");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            data.depthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth");

            data.albedoRT = builder.WriteColor(0, data.albedoRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.normalRT = builder.WriteColor(1, data.normalRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.emissiveRT = builder.WriteColor(2, data.emissiveRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.depthRT = builder.WriteDepth(data.depthRT, 0, GfxRenderPassLoadOp::Clear);
        },
        [&](const GBufferPassData& data, IGfxCommandList* pCommandList)
        {
            World* world = Engine::GetInstance()->GetWorld();
            Camera* camera = world->GetCamera();
            ILight* light = world->GetPrimaryLight();

            CameraConstant cameraCB;
            cameraCB.cameraPos = camera->GetPosition();
            pCommandList->SetGraphicsConstants(3, &cameraCB, sizeof(cameraCB));

            SceneConstant sceneCB;
            sceneCB.lightDir = light->GetLightDirection();
            //sceneCB.shadowRT = shadowMapRT->GetSRV()->GetHeapIndex();
            sceneCB.lightColor = light->GetLightColor() * light->GetLightIntensity();
            sceneCB.shadowSampler = m_pShadowSampler->GetHeapIndex();
            sceneCB.mtxLightVP = GetLightVP(light);
            sceneCB.pointRepeatSampler = m_pPointRepeatSampler->GetHeapIndex();
            sceneCB.pointClampSampler = m_pPointClampSampler->GetHeapIndex();
            sceneCB.linearRepeatSampler = m_pLinearRepeatSampler->GetHeapIndex();
            sceneCB.linearClampSampler = m_pLinearClampSampler->GetHeapIndex();
            sceneCB.aniso2xSampler = m_pAniso2xSampler->GetHeapIndex();
            sceneCB.aniso4xSampler = m_pAniso4xSampler->GetHeapIndex();
            sceneCB.aniso8xSampler = m_pAniso8xSampler->GetHeapIndex();
            sceneCB.aniso16xSampler = m_pAniso16xSampler->GetHeapIndex();
            sceneCB.envTexture = m_pEnvTexture->GetSRV()->GetHeapIndex();
            sceneCB.brdfTexture = m_pBrdfTexture->GetSRV()->GetHeapIndex();

            pCommandList->SetGraphicsConstants(4, &sceneCB, sizeof(sceneCB));

            for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
            {
                //m_basePassBatchs[i](pCommandList, this, camera);
            }
            m_basePassBatchs.clear();
        });

    struct ClusterShadingPassData
    {
        RenderGraphHandle albedoRT;
        RenderGraphHandle normalRT;
        RenderGraphHandle emissiveRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle shadowRT;

        RenderGraphHandle hdrRT;
    };

    auto cluster_shading_pass = m_pRenderGraph->AddPass<ClusterShadingPassData>("ClusterShading",
        [&](ClusterShadingPassData& data, RenderGraphBuilder& builder)
        {
            data.albedoRT = builder.Read(gbuffer_pass->albedoRT, GfxResourceState::ShaderResourceNonPS);
            data.normalRT = builder.Read(gbuffer_pass->normalRT, GfxResourceState::ShaderResourceNonPS);
            data.emissiveRT = builder.Read(gbuffer_pass->emissiveRT, GfxResourceState::ShaderResourceNonPS);
            data.depthRT = builder.Read(gbuffer_pass->depthRT, GfxResourceState::ShaderResourceNonPS);
            data.shadowRT = builder.Read(shadow_pass->depthRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.hdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor");

            data.hdrRT = builder.Write(data.hdrRT, GfxResourceState::UnorderedAccess);
        },
        [&](const ClusterShadingPassData& data, IGfxCommandList* pCommandList)
        {
            m_pClusteredShading->Draw(pCommandList);
        }
        );

    struct ForwardPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle depthRT;
    };

    //todo : forward pass

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle ldrRT;
    };

    auto tonemap_pass = m_pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.hdrRT = builder.Read(cluster_shading_pass->hdrRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.ldrRT = builder.Create<RenderGraphTexture>(desc, "ToneMapping Output");
            data.ldrRT = builder.Write(data.ldrRT, GfxResourceState::UnorderedAccess);
        },
        [&](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* sceneColorRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            m_pToneMapper->Draw(pCommandList, sceneColorRT->GetSRV(), ldrRT->GetUAV(), m_nWindowWidth, m_nWindowHeight);
        });

    struct FXAAPassData
    {
        RenderGraphHandle ldrRT;
        RenderGraphHandle outputRT;
    };

    auto fxaa_pass = m_pRenderGraph->AddPass<FXAAPassData>("FXAA",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            data.ldrRT = builder.Read(tonemap_pass->ldrRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "FXAA Output");
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);
        },
        [&](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);
            RenderGraphTexture* outRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.outputRT);

            m_pFXAA->Draw(pCommandList, ldrRT->GetSRV(), outRT->GetUAV(), m_nWindowWidth, m_nWindowHeight);
        });

    return fxaa_pass->outputRT;
}