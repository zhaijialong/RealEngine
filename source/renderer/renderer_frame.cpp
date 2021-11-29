#include "renderer.h"
#include "core/engine.h"

void Renderer::BuildRenderGraph(RenderGraphHandle& outColor, RenderGraphHandle& outDepth)
{
    struct GBufferPassData
    {
        RenderGraphHandle outAlbedoRT; //srgb : albedo(xyz) + metalness(a)
        RenderGraphHandle outNormalRT; //rgba8norm : normal(xyz) + roughness(a)
        RenderGraphHandle outEmissiveRT; //srgb : emissive(xyz) + AO(a) 
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

            desc.format = GfxFormat::RGBA8UNORM;
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

            for (size_t i = 0; i < m_gbufferPassBatchs.size(); ++i)
            {
                m_gbufferPassBatchs[i](pCommandList, world->GetCamera());
            }
            m_gbufferPassBatchs.clear();
        });

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
            World* world = Engine::GetInstance()->GetWorld();
            Camera* camera = world->GetCamera();
            ILight* light = world->GetPrimaryLight();

            float4x4 mtxShadowViewProjection = light->GetShadowMatrix();
            pCommandList->SetGraphicsConstants(3, &mtxShadowViewProjection, sizeof(float4x4));

            for (size_t i = 0; i < m_shadowPassBatchs.size(); ++i)
            {
                m_shadowPassBatchs[i](pCommandList, light);
            }
            m_shadowPassBatchs.clear();

            camera->SetupCameraCB(pCommandList);
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

                m_forwardPassBatchs[i](pCommandList, world->GetCamera());
            }
            m_forwardPassBatchs.clear();
        });

    struct VelocityPassData
    {
        RenderGraphHandle outVelocityRT;
        RenderGraphHandle outSceneDepthRT;
    };

    auto velocity_pass = m_pRenderGraph->AddPass<VelocityPassData>("Velocity Pass",
        [&](VelocityPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            desc.format = GfxFormat::RGBA16F;
            data.outVelocityRT = builder.Create<RenderGraphTexture>(desc, "Velocity RT");

            data.outVelocityRT = builder.WriteColor(0, data.outVelocityRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0));
            data.outSceneDepthRT = builder.WriteDepth(forward_pass->outSceneDepthRT, 0, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load);
        },
        [&](const VelocityPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_velocityPassBatchs.size(); ++i)
            {
                World* world = Engine::GetInstance()->GetWorld();

                m_velocityPassBatchs[i](pCommandList, world->GetCamera());
            }
            m_velocityPassBatchs.clear();
        });


    struct LinearizeDepthPassData
    {
        RenderGraphHandle inputDepthRT;
        RenderGraphHandle outputLinearDepthRT;
    };

    auto linearize_depth_pass = m_pRenderGraph->AddPass<LinearizeDepthPassData>("Linearize Depth",
        [&](LinearizeDepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.usage = GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource;
            desc.format = GfxFormat::R32F;
            data.outputLinearDepthRT = builder.Create<RenderGraphTexture>(desc, "LinearDepth RT");

            data.inputDepthRT = builder.Read(velocity_pass->outSceneDepthRT, GfxResourceState::ShaderResourceNonPS);
            data.outputLinearDepthRT = builder.Write(data.outputLinearDepthRT, GfxResourceState::UnorderedAccess);
        },
        [&](const LinearizeDepthPassData& data, IGfxCommandList* pCommandList)
        {
            static IGfxPipelineState* pPSO = nullptr;
            if (pPSO == nullptr)
            {
                GfxComputePipelineDesc psoDesc;
                psoDesc.cs = GetShader("linearize_depth.hlsl", "main", "cs_6_6", {});
                pPSO = GetPipelineState(psoDesc, "LinearizeDepth PSO");
            }

            pCommandList->SetPipelineState(pPSO);

            RenderGraphTexture* inputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.inputDepthRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.outputLinearDepthRT);
            uint32_t cb[4] = { inputRT->GetSRV()->GetHeapIndex(), outputRT->GetUAV()->GetHeapIndex(), 0, 0 };
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));

            pCommandList->Dispatch((m_nWindowWidth + 7) / 8, (m_nWindowHeight + 7) / 8, 1);
        });

    PostProcessInput ppInput;
    ppInput.sceneColorRT = forward_pass->outSceneColorRT;
    ppInput.sceneDepthRT = velocity_pass->outSceneDepthRT;
    ppInput.linearDepthRT = linearize_depth_pass->outputLinearDepthRT;
    ppInput.velocityRT = velocity_pass->outVelocityRT;

    RenderGraphHandle output = m_pPostProcessor->Process(m_pRenderGraph.get(), ppInput, m_nWindowWidth, m_nWindowHeight);

    struct CopyPassData
    {
        RenderGraphHandle srcTexture;
        RenderGraphHandle dstTexture;
    };

    m_pRenderGraph->AddPass<CopyPassData>("Copy Depth",
        [&](CopyPassData& data, RenderGraphBuilder& builder)
        {
            data.srcTexture = builder.Read(linearize_depth_pass->outputLinearDepthRT, GfxResourceState::CopySrc);
            data.dstTexture = builder.Write(m_prevLinearDepthHandle, GfxResourceState::CopyDst);

            builder.MakeTarget();
        },
        [&](const CopyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* srcTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcTexture);

            pCommandList->CopyTexture(m_pPrevLinearDepthTexture->GetTexture(), srcTexture->GetTexture());
        });

    outColor = output;
    outDepth = ppInput.sceneDepthRT;

    m_pRenderGraph->Present(outColor, GfxResourceState::ShaderResourcePS);
    m_pRenderGraph->Present(outDepth, GfxResourceState::DepthStencilReadOnly);
}