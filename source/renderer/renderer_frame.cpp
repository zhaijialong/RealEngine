#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "base_pass.h"
#include "path_tracer.h"
#include "lighting/lighting_processor.h"
#include "post_processing/post_processor.h"
#include "core/engine.h"

void Renderer::BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth)
{
    m_pHZB->Generate1stPhaseCullingHZB(m_pRenderGraph.get());
    m_pBasePass->Render1stPhase(m_pRenderGraph.get());

    m_pHZB->Generate2ndPhaseCullingHZB(m_pRenderGraph.get(), m_pBasePass->GetDepthRT());
    m_pBasePass->Render2ndPhase(m_pRenderGraph.get());

    RGHandle sceneDepthRT = m_pBasePass->GetDepthRT();
    RGHandle velocityRT = VelocityPass(sceneDepthRT);
    RGHandle linearDepthRT = LinearizeDepthPass(sceneDepthRT);

    m_pHZB->GenerateSceneHZB(m_pRenderGraph.get(), sceneDepthRT);

    RGHandle sceneColorRT;
    if (m_outputType == RendererOutput::PathTracing)
    {
        sceneColorRT = m_pPathTracer->AddPass(m_pRenderGraph.get(), sceneDepthRT, m_nRenderWidth, m_nRenderHeight);
    }
    else
    {
        sceneColorRT = m_pLightingProcessor->AddPass(m_pRenderGraph.get(), sceneDepthRT, linearDepthRT, velocityRT, m_nRenderWidth, m_nRenderHeight);

        ForwardPass(sceneColorRT, sceneDepthRT);
    }

    RGHandle output = m_pPostProcessor->AddPass(m_pRenderGraph.get(), sceneColorRT, sceneDepthRT, velocityRT,
        m_nRenderWidth, m_nRenderHeight, m_nDisplayWidth, m_nDisplayHeight);

    ObjectIDPass(sceneDepthRT);
    CopyHistoryPass(sceneDepthRT, m_pBasePass->GetNormalRT(), sceneColorRT);

    outColor = output;
    outDepth = sceneDepthRT;

    m_pRenderGraph->Present(outColor, GfxAccessPixelShaderSRV);
    m_pRenderGraph->Present(outDepth, GfxAccessDSVReadOnly);
}

void Renderer::ForwardPass(RGHandle& color, RGHandle& depth)
{
    struct ForwardPassData
    {
        RGHandle outSceneColorRT;
        RGHandle outSceneDepthRT;
    };

    auto forward_pass = m_pRenderGraph->AddPass<ForwardPassData>("Forward Pass", RenderPassType::Graphics,
        [&](ForwardPassData& data, RGBuilder& builder)
        {
            data.outSceneColorRT = builder.WriteColor(0, color, 0, GfxRenderPassLoadOp::Load);
            data.outSceneDepthRT = builder.WriteDepth(depth, 0, GfxRenderPassLoadOp::Load);
        },
        [&](const ForwardPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_forwardPassBatchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_forwardPassBatchs[i]);
            }
        });

    color = forward_pass->outSceneColorRT;
    depth = forward_pass->outSceneDepthRT;
}

RGHandle Renderer::VelocityPass(RGHandle& depth)
{
    RENDER_GRAPH_EVENT(m_pRenderGraph.get(), "Velocity Pass");

    struct ObjectVelocityPassData
    {
        RGHandle outVelocityRT;
        RGHandle outSceneDepthRT;
    };

    auto obj_velocity_pass = m_pRenderGraph->AddPass<ObjectVelocityPassData>("Object Velocity", RenderPassType::Graphics,
        [&](ObjectVelocityPassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.width = m_nRenderWidth;
            desc.height = m_nRenderHeight;
            desc.format = GfxFormat::RGBA16F;
            data.outVelocityRT = builder.Create<RGTexture>(desc, "Velocity RT");

            data.outVelocityRT = builder.WriteColor(0, data.outVelocityRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0));
            data.outSceneDepthRT = builder.WriteDepth(depth, 0, GfxRenderPassLoadOp::Load);
        },
        [&](const ObjectVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_velocityPassBatchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_velocityPassBatchs[i]);
            }
        });

    struct CameraVelocityPassData
    {
        RGHandle velocity;
        RGHandle depth;
    };

    auto camera_velocity_pass = m_pRenderGraph->AddPass<CameraVelocityPassData>("Camera Velocity", RenderPassType::Compute,
        [&](CameraVelocityPassData& data, RGBuilder& builder)
        {
            data.velocity = builder.Write(obj_velocity_pass->outVelocityRT);
            data.depth = builder.Read(obj_velocity_pass->outSceneDepthRT);
        },
        [=](const CameraVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            GfxComputePipelineDesc psoDesc;
            psoDesc.cs = GetShader("velocity.hlsl", "main", "cs_6_6", {});
            IGfxPipelineState* pPSO = GetPipelineState(psoDesc, "Velocity PSO");

            pCommandList->SetPipelineState(pPSO);

            RGTexture* velocity = m_pRenderGraph->GetTexture(data.velocity);
            RGTexture* depth = m_pRenderGraph->GetTexture(data.depth);
            uint32_t cb[2] = { velocity->GetUAV()->GetHeapIndex(), depth->GetSRV()->GetHeapIndex()};
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));

            pCommandList->Dispatch(DivideRoudingUp(m_nRenderWidth, 8), DivideRoudingUp(m_nRenderHeight, 8), 1);
        });

    depth = camera_velocity_pass->depth;
    return camera_velocity_pass->velocity;
}

RGHandle Renderer::LinearizeDepthPass(RGHandle depth)
{
    struct LinearizeDepthPassData
    {
        RGHandle inputDepthRT;
        RGHandle outputLinearDepthRT;
    };

    auto linearize_depth_pass = m_pRenderGraph->AddPass<LinearizeDepthPassData>("Linearize Depth", RenderPassType::Compute,
        [&](LinearizeDepthPassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.width = m_nRenderWidth;
            desc.height = m_nRenderHeight;
            desc.format = GfxFormat::R32F;
            data.outputLinearDepthRT = builder.Create<RGTexture>(desc, "LinearDepth RT");

            data.inputDepthRT = builder.Read(depth);
            data.outputLinearDepthRT = builder.Write(data.outputLinearDepthRT);
        },
        [&](const LinearizeDepthPassData& data, IGfxCommandList* pCommandList)
        {
            GfxComputePipelineDesc psoDesc;
            psoDesc.cs = GetShader("linearize_depth.hlsl", "main", "cs_6_6", {});
            IGfxPipelineState* pPSO = GetPipelineState(psoDesc, "LinearizeDepth PSO");

            pCommandList->SetPipelineState(pPSO);

            RGTexture* inputRT = m_pRenderGraph->GetTexture(data.inputDepthRT);
            RGTexture* outputRT = m_pRenderGraph->GetTexture(data.outputLinearDepthRT);
            uint32_t cb[2] = { inputRT->GetSRV()->GetHeapIndex(), outputRT->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));

            pCommandList->Dispatch(DivideRoudingUp(m_nRenderWidth, 8), DivideRoudingUp(m_nRenderHeight, 8), 1);
        });

    return linearize_depth_pass->outputLinearDepthRT;
}

void Renderer::ObjectIDPass(RGHandle& depth)
{
    if (m_bEnableObjectIDRendering)
    {
        struct IDPassData
        {
            RGHandle idTexture;
            RGHandle sceneDepthTexture;
        };

        auto id_pass = m_pRenderGraph->AddPass<IDPassData>("Object ID Pass", RenderPassType::Graphics,
            [&](IDPassData& data, RGBuilder& builder)
            {
                RGTexture::Desc desc;
                desc.width = m_nRenderWidth;
                desc.height = m_nRenderHeight;
                desc.format = GfxFormat::R32UI;
                data.idTexture = builder.Create<RGTexture>(desc, "Object ID");

                data.idTexture = builder.WriteColor(0, data.idTexture, 0, GfxRenderPassLoadOp::Clear, float4(1000000, 0, 0, 0));
                data.sceneDepthTexture = builder.ReadDepth(depth, 0);
            },
            [&](const IDPassData& data, IGfxCommandList* pCommandList)
            {
                World* world = Engine::GetInstance()->GetWorld();

                for (size_t i = 0; i < m_idPassBatchs.size(); ++i)
                {
                    DrawBatch(pCommandList, m_idPassBatchs[i]);
                }
            });

        depth = id_pass->sceneDepthTexture;

        struct CopyIDPassData
        {
            RGHandle srcTexture;
        };

        m_pRenderGraph->AddPass<CopyIDPassData>("Copy ID to Readback Buffer", RenderPassType::Copy,
            [&](CopyIDPassData& data, RGBuilder& builder)
            {
                data.srcTexture = builder.Read(id_pass->idTexture);

                builder.SkipCulling();
            },
            [&](const CopyIDPassData& data, IGfxCommandList* pCommandList)
            {
                RGTexture* srcTexture = m_pRenderGraph->GetTexture(data.srcTexture);

                m_nObjectIDRowPitch = srcTexture->GetTexture()->GetRowPitch();
                uint32_t size = m_nObjectIDRowPitch * srcTexture->GetTexture()->GetDesc().height;
                if (m_pObjectIDBuffer == nullptr || m_pObjectIDBuffer->GetDesc().size < size)
                {
                    GfxBufferDesc desc;
                    desc.size = size;
                    desc.memory_type = GfxMemoryType::GpuToCpu;
                    m_pObjectIDBuffer.reset(m_pDevice->CreateBuffer(desc, "Renderer::m_pObjectIDBuffer"));
                }

                pCommandList->CopyTextureToBuffer(m_pObjectIDBuffer.get(), 0, srcTexture->GetTexture(), 0, 0);
            });
    }
}

void Renderer::CopyHistoryPass(RGHandle sceneDepth, RGHandle normal, RGHandle sceneColor)
{
    struct CopyPassData
    {
        RGHandle srcSceneDepthTexture;
        RGHandle dstSceneDepthTexture;

        RGHandle srcNormalTexture;
        RGHandle dstNormalTexture;

        RGHandle srcSceneColorTexture;
        RGHandle dstSceneColorTexture;
    };

    m_pRenderGraph->AddPass<CopyPassData>("Copy History Textures", RenderPassType::Copy,
        [&](CopyPassData& data, RGBuilder& builder)
        {
            data.srcSceneDepthTexture = builder.Read(sceneDepth);
            data.dstSceneDepthTexture = builder.Write(m_prevSceneDepthHandle);

            data.srcNormalTexture = builder.Read(normal);
            data.dstNormalTexture = builder.Write(m_prevNormalHandle);

            data.srcSceneColorTexture = builder.Read(sceneColor);
            data.dstSceneColorTexture = builder.Write(m_prevSceneColorHandle);

            builder.SkipCulling();
        },
        [&](const CopyPassData& data, IGfxCommandList* pCommandList)
        {
            RGTexture* srcSceneDepthTexture = m_pRenderGraph->GetTexture(data.srcSceneDepthTexture);
            RGTexture* srcNormalTexture = m_pRenderGraph->GetTexture(data.srcNormalTexture);
            RGTexture* srcSceneColorTexture = m_pRenderGraph->GetTexture(data.srcSceneColorTexture);

            pCommandList->CopyTexture(m_pPrevSceneDepthTexture->GetTexture(), 0, 0, srcSceneDepthTexture->GetTexture(), 0, 0);
            pCommandList->CopyTexture(m_pPrevNormalTexture->GetTexture(), 0, 0, srcNormalTexture->GetTexture(), 0, 0);
            pCommandList->CopyTexture(m_pPrevSceneColorTexture->GetTexture(), 0, 0, srcSceneColorTexture->GetTexture(), 0, 0);
        });
}
