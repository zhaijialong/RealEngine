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

    struct CSMPassData
    {
        RenderGraphHandle initialRT;

        RenderGraphHandle depthRT;
    };

    auto csm_pass0 = m_pRenderGraph->AddPass<CSMPassData>("CSM Pass 0",
        [](CSMPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = desc.height = 4096;
            desc.format = GfxFormat::D16;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            desc.type = GfxTextureType::Texture2DArray;
            desc.array_size = 2;

            data.initialRT = builder.Create<RenderGraphTexture>(desc, "CSM RT");
            data.depthRT = builder.WriteDepth(data.initialRT, 0, GfxRenderPassLoadOp::Clear, 1.0f);
        },
        [&](const CSMPassData& data, IGfxCommandList* pCommandList)
        {
        });

    auto csm_pass1 = m_pRenderGraph->AddPass<DepthPassData>("CSM Pass 1",
        [&](DepthPassData& data, RenderGraphBuilder& builder)
        {
            data.depthRT = builder.WriteDepth(csm_pass0->initialRT, 1, GfxRenderPassLoadOp::Clear, 1.0f);
        },
        [&](const DepthPassData& data, IGfxCommandList* pCommandList)
        {
        });

    struct BassPassData
    {
        RenderGraphHandle shadowRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle hdrRT;

        RenderGraphHandle csmRT;
    };

    auto base_pass = m_pRenderGraph->AddPass<BassPassData>("Base Pass",
        [&](BassPassData& data, RenderGraphBuilder& builder)
        {
            data.shadowRT = builder.Read(shadow_pass->depthRT, GfxResourceState::ShaderResourcePSOnly);

            data.csmRT = builder.Read(csm_pass0->depthRT, GfxResourceState::ShaderResourcePSOnly, 0);
            data.csmRT = builder.Read(csm_pass1->depthRT, GfxResourceState::ShaderResourcePSOnly, 1);

            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.hdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            data.depthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth");

            data.hdrRT = builder.WriteColor(0, data.hdrRT, 0, GfxRenderPassLoadOp::Clear);
            data.depthRT = builder.WriteDepth(data.depthRT, 0, GfxRenderPassLoadOp::Clear, GfxRenderPassLoadOp::Clear);
        },
        [&](const BassPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* shadowMapRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.shadowRT);

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
            sceneCB.aniso2xSampler = m_pAniso2xSampler->GetHeapIndex();
            sceneCB.aniso4xSampler = m_pAniso4xSampler->GetHeapIndex();
            sceneCB.aniso8xSampler = m_pAniso8xSampler->GetHeapIndex();
            sceneCB.aniso16xSampler = m_pAniso16xSampler->GetHeapIndex();
            sceneCB.envTexture = m_pEnvTexture->GetSRV()->GetHeapIndex();
            sceneCB.brdfTexture = m_pBrdfTexture->GetSRV()->GetHeapIndex();

            pCommandList->SetGraphicsConstants(4, &sceneCB, sizeof(sceneCB));

            for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
            {
                m_basePassBatchs[i](pCommandList, this, camera);
            }
            m_basePassBatchs.clear();
        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle ldrRT;
    };

    auto tonemap_pass = m_pRenderGraph->AddPass<TonemapPassData>("ToneMapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            data.hdrRT = builder.Read(base_pass->hdrRT, GfxResourceState::ShaderResource);

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

            uint32_t width = sceneColorRT->GetTexture()->GetDesc().width;
            uint32_t height = sceneColorRT->GetTexture()->GetDesc().height;

            m_pToneMap->Draw(pCommandList, sceneColorRT->GetSRV(), ldrRT->GetUAV(), width, height);
        });

    struct FXAAPassData
    {
        RenderGraphHandle ldrRT;
        RenderGraphHandle outputRT;
    };

    auto fxaa_pass = m_pRenderGraph->AddPass<FXAAPassData>("FXAA",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            data.ldrRT = builder.Read(tonemap_pass->ldrRT, GfxResourceState::ShaderResourcePSOnly);

            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "FXAA Output");
            data.outputRT = builder.WriteColor(0, data.outputRT, 0, GfxRenderPassLoadOp::DontCare);
        },
        [&](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            m_pFXAA->Draw(pCommandList, ldrRT->GetSRV(), m_nWindowWidth, m_nWindowHeight);
        });

    return fxaa_pass->outputRT;
}