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
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [&](const DepthPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* shadowRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.depthRT);

            GfxRenderPassDesc shadow_pass;
            shadow_pass.depth.texture = shadowRT->GetTexture();
            shadow_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
            shadow_pass.depth.clear_depth = 1.0f;
            pCommandList->BeginRenderPass(shadow_pass);

            ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
            float4x4 mtxVP = GetLightVP(light);

            for (size_t i = 0; i < m_shadowPassBatchs.size(); ++i)
            {
                m_shadowPassBatchs[i](pCommandList, this, mtxVP);
            }
            m_shadowPassBatchs.clear();

            pCommandList->EndRenderPass();
        });

    struct BassPassData
    {
        RenderGraphHandle shadowRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle hdrRT;
    };

    auto base_pass = m_pRenderGraph->AddPass<BassPassData>("Base Pass",
        [&](BassPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.hdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            data.depthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth");

            data.shadowRT = builder.Read(shadow_pass->depthRT, GfxResourceState::ShaderResourcePSOnly);

            data.hdrRT = builder.Write(data.hdrRT, GfxResourceState::RenderTarget);
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [&](const BassPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* shadowMapRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.shadowRT);
            RenderGraphTexture* sceneColorRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* sceneDepthRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.depthRT);

            World* world = Engine::GetInstance()->GetWorld();
            Camera* camera = world->GetCamera();
            ILight* light = world->GetPrimaryLight();

            CameraConstant cameraCB;
            cameraCB.cameraPos = camera->GetPosition();
            pCommandList->SetGraphicsConstants(3, &cameraCB, sizeof(cameraCB));

            SceneConstant sceneCB;
            sceneCB.lightDir = light->GetLightDirection();
            sceneCB.shadowRT = shadowMapRT->GetSRV()->GetHeapIndex();
            sceneCB.lightColor = light->GetLightColor() * light->GetLightIntensity();
            sceneCB.shadowSampler = m_pShadowSampler->GetHeapIndex();
            sceneCB.mtxLightVP = GetLightVP(light);
            sceneCB.pointRepeatSampler = m_pPointRepeatSampler->GetHeapIndex();
            sceneCB.pointClampSampler = m_pPointClampSampler->GetHeapIndex();
            sceneCB.linearRepeatSampler = m_pLinearRepeatSampler->GetHeapIndex();
            sceneCB.linearClampSampler = m_pLinearClampSampler->GetHeapIndex();
            sceneCB.envTexture = m_pEnvTexture->GetSRV()->GetHeapIndex();
            sceneCB.brdfTexture = m_pBrdfTexture->GetSRV()->GetHeapIndex();

            pCommandList->SetGraphicsConstants(4, &sceneCB, sizeof(sceneCB));

            GfxRenderPassDesc render_pass;
            render_pass.color[0].texture = sceneColorRT->GetTexture();
            render_pass.color[0].load_op = GfxRenderPassLoadOp::Clear;
            render_pass.color[0].clear_color[0] = 0.0f;
            render_pass.color[0].clear_color[1] = 0.0f;
            render_pass.color[0].clear_color[2] = 0.0f;
            render_pass.color[0].clear_color[3] = 1.0f;
            render_pass.depth.texture = sceneDepthRT->GetTexture();
            render_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
            render_pass.depth.stencil_load_op = GfxRenderPassLoadOp::Clear;
            pCommandList->BeginRenderPass(render_pass);

            for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
            {
                m_basePassBatchs[i](pCommandList, this, camera);
            }
            m_basePassBatchs.clear();

            pCommandList->EndRenderPass();
        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle ldrRT;
    };

    auto tonemap_pass = m_pRenderGraph->AddPass<TonemapPassData>("Tonemapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.ldrRT = builder.Create<RenderGraphTexture>(desc, "Tonemapping Output");

            data.hdrRT = builder.Read(base_pass->hdrRT, GfxResourceState::ShaderResourcePSOnly);
            data.ldrRT = builder.Write(data.ldrRT, GfxResourceState::RenderTarget);
        },
        [&](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* sceneColorRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            GfxRenderPassDesc pass;
            pass.color[0].texture = ldrRT->GetTexture();
            pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
            pCommandList->BeginRenderPass(pass);

            m_pToneMap->Draw(pCommandList, sceneColorRT->GetSRV());

            pCommandList->EndRenderPass();
        });

    struct FXAAPassData
    {
        RenderGraphHandle ldrRT;
        RenderGraphHandle outputRT;
    };

    auto fxaa_pass = m_pRenderGraph->AddPass<FXAAPassData>("FXAA",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "FXAA Output");

            data.ldrRT = builder.Read(tonemap_pass->ldrRT, GfxResourceState::ShaderResourcePSOnly);
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::RenderTarget);
        },
        [&](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* outputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.outputRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            GfxRenderPassDesc pass;
            pass.color[0].texture = outputRT->GetTexture();
            pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
            pCommandList->BeginRenderPass(pass);

            m_pFXAA->Draw(pCommandList, ldrRT->GetSRV(), m_nWindowWidth, m_nWindowHeight);

            pCommandList->EndRenderPass();
        });

    return fxaa_pass->outputRT;
}