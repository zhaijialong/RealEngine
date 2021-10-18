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
        RenderGraphHandle outDepthRT;
    };

    auto shadow_pass = m_pRenderGraph->AddPass<DepthPassData>("Shadow Pass",
        [](DepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = desc.height = 4096;
            desc.format = GfxFormat::D16;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;

            data.outDepthRT = builder.Create<RenderGraphTexture>(desc, "ShadowMap");
            data.outDepthRT = builder.WriteDepth(data.outDepthRT, 0, GfxRenderPassLoadOp::Clear, 1.0f);
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
        RenderGraphHandle outAlbedoRT; //srgb : albedo(xyz) + AO(a) 
        RenderGraphHandle outNormalRT; //rgb10a2 : normal(xy) + roughness(z)
        RenderGraphHandle outEmissiveRT; //srgb : emissive(xyz) + metalness(a)
        RenderGraphHandle outDepthRT;
    };

    auto gbuffer_pass = m_pRenderGraph->AddPass<GBufferPassData>("GBuffer Pass",
        [&](GBufferPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            desc.format = GfxFormat::RGBA8SRGB;
            data.outAlbedoRT = builder.Create<RenderGraphTexture>(desc, "Albedo RT");
            data.outEmissiveRT = builder.Create<RenderGraphTexture>(desc, "Emissive RT");

            desc.format = GfxFormat::RGB10A2UNORM;
            data.outNormalRT = builder.Create<RenderGraphTexture>(desc, "Normal RT");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            data.outDepthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth RT");

            data.outAlbedoRT = builder.WriteColor(0, data.outAlbedoRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outNormalRT = builder.WriteColor(1, data.outNormalRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outEmissiveRT = builder.WriteColor(2, data.outEmissiveRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0f));
            data.outDepthRT = builder.WriteDepth(data.outDepthRT, 0, GfxRenderPassLoadOp::Clear, GfxRenderPassLoadOp::Clear);
        },
        [&](const GBufferPassData& data, IGfxCommandList* pCommandList)
        {
            World* world = Engine::GetInstance()->GetWorld();
            Camera* camera = world->GetCamera();
            ILight* light = world->GetPrimaryLight();

            CameraConstant cameraCB;
            cameraCB.cameraPos = camera->GetPosition();
            pCommandList->SetGraphicsConstants(3, &cameraCB, sizeof(cameraCB));
            pCommandList->SetComputeConstants(3, &cameraCB, sizeof(cameraCB));

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
            pCommandList->SetComputeConstants(4, &sceneCB, sizeof(sceneCB));

            for (size_t i = 0; i < m_gbufferPassBatchs.size(); ++i)
            {
                m_gbufferPassBatchs[i](pCommandList, this, camera);
            }
            m_gbufferPassBatchs.clear();
        });

    LightingProcessInput lightInput;
    lightInput.albedoRT = gbuffer_pass->outAlbedoRT;
    lightInput.normalRT = gbuffer_pass->outNormalRT;
    lightInput.emissiveRT = gbuffer_pass->outEmissiveRT;
    lightInput.depthRT = gbuffer_pass->outDepthRT;
    lightInput.shadowRT = shadow_pass->outDepthRT;

    RenderGraphHandle lightRT = m_pLightingProcessor->Process(m_pRenderGraph.get(), lightInput, m_nWindowWidth, m_nWindowHeight);

    struct ForwardPassData
    {
        RenderGraphHandle outSceneColorRT;
        RenderGraphHandle outSceneDepthRT;
    };

    auto forward_pass = m_pRenderGraph->AddPass<ForwardPassData>("Forward Pass",
        [&](ForwardPassData& data, RenderGraphBuilder& builder)
        {
            data.outSceneColorRT = builder.WriteColor(0, lightRT, 0, GfxRenderPassLoadOp::Load);
            data.outSceneDepthRT = builder.WriteDepth(gbuffer_pass->outDepthRT, 0, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load);
        },
        [&](const ForwardPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_forwardPassBatchs.size(); ++i)
            {
                World* world = Engine::GetInstance()->GetWorld();
                Camera* camera = world->GetCamera();

                m_forwardPassBatchs[i](pCommandList, this, camera);
            }
            m_forwardPassBatchs.clear();
        });


    PostProcessInput ppInput;
    ppInput.sceneColorRT = forward_pass->outSceneColorRT;
    ppInput.sceneDepthRT = forward_pass->outSceneDepthRT;

    RenderGraphHandle output = m_pPostProcessor->Process(m_pRenderGraph.get(), ppInput, m_nWindowWidth, m_nWindowHeight);

    return output;
}