#include "renderer.h"
#include "hierarchical_depth_buffer.h"
#include "base_pass.h"
#include "path_tracer.h"
#include "lighting/lighting_processor.h"
#include "post_processing/post_processor.h"
#include "core/engine.h"

void Renderer::BuildRenderGraph(RenderGraphHandle& outColor, RenderGraphHandle& outDepth)
{
    m_pHZB->Generate1stPhaseCullingHZB(m_pRenderGraph.get());
    m_pBasePass->Render1stPhase(m_pRenderGraph.get());

    m_pHZB->Generate2ndPhaseCullingHZB(m_pRenderGraph.get(), m_pBasePass->GetDepthRT());
    m_pBasePass->Render2ndPhase(m_pRenderGraph.get());

    RenderGraphHandle sceneDepthRT = m_pBasePass->GetDepthRT();
    RenderGraphHandle velocityRT = VelocityPass(sceneDepthRT);
    RenderGraphHandle linearDepthRT = LinearizeDepthPass(sceneDepthRT);

    m_pHZB->GenerateSceneHZB(m_pRenderGraph.get(), sceneDepthRT);

    RenderGraphHandle sceneColorRT;
    if (m_outputType == RendererOutput::PathTracing)
    {
        sceneColorRT = m_pPathTracer->Render(m_pRenderGraph.get(), sceneDepthRT, m_nWindowWidth, m_nWindowHeight);
    }
    else
    {
        sceneColorRT = m_pLightingProcessor->Render(m_pRenderGraph.get(), sceneDepthRT, linearDepthRT, velocityRT, m_nWindowWidth, m_nWindowHeight);

        ForwardPass(sceneColorRT, sceneDepthRT);
    }

    RenderGraphHandle output = m_pPostProcessor->Render(m_pRenderGraph.get(), sceneColorRT, sceneDepthRT, linearDepthRT, velocityRT, m_nWindowWidth, m_nWindowHeight);

    ObjectIDPass(sceneDepthRT);
    CopyHistoryPass(linearDepthRT, m_pBasePass->GetNormalRT(), sceneColorRT);

    outColor = output;
    outDepth = sceneDepthRT;

    m_pRenderGraph->Present(outColor, GfxResourceState::ShaderResourcePS);
    m_pRenderGraph->Present(outDepth, GfxResourceState::DepthStencilReadOnly);
}

void Renderer::ForwardPass(RenderGraphHandle& color, RenderGraphHandle& depth)
{
    struct ForwardPassData
    {
        RenderGraphHandle outSceneColorRT;
        RenderGraphHandle outSceneDepthRT;
    };

    auto forward_pass = m_pRenderGraph->AddPass<ForwardPassData>("Forward Pass", RenderPassType::Graphics,
        [&](ForwardPassData& data, RenderGraphBuilder& builder)
        {
            data.outSceneColorRT = builder.WriteColor(0, color, 0, GfxRenderPassLoadOp::Load);
            data.outSceneDepthRT = builder.ReadDepth(depth, 0);
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

RenderGraphHandle Renderer::VelocityPass(RenderGraphHandle& depth)
{
    RENDER_GRAPH_EVENT(m_pRenderGraph.get(), "Velocity Pass");

    struct ObjectVelocityPassData
    {
        RenderGraphHandle outVelocityRT;
        RenderGraphHandle outSceneDepthRT;
    };

    auto obj_velocity_pass = m_pRenderGraph->AddPass<ObjectVelocityPassData>("Object Velocity", RenderPassType::Graphics,
        [&](ObjectVelocityPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RG16F;
            data.outVelocityRT = builder.Create<RenderGraphTexture>(desc, "Velocity RT");

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
        RenderGraphHandle velocity;
        RenderGraphHandle depth;
    };

    auto camera_velocity_pass = m_pRenderGraph->AddPass<CameraVelocityPassData>("Camera Velocity", RenderPassType::Compute,
        [&](CameraVelocityPassData& data, RenderGraphBuilder& builder)
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

            RenderGraphTexture* velocity = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* depth = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.depth);
            uint32_t cb[2] = { velocity->GetUAV()->GetHeapIndex(), depth->GetSRV()->GetHeapIndex()};
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));

            pCommandList->Dispatch((m_nWindowWidth + 7) / 8, (m_nWindowHeight + 7) / 8, 1);
        });

    depth = camera_velocity_pass->depth;
    return camera_velocity_pass->velocity;
}

RenderGraphHandle Renderer::LinearizeDepthPass(RenderGraphHandle depth)
{
    struct LinearizeDepthPassData
    {
        RenderGraphHandle inputDepthRT;
        RenderGraphHandle outputLinearDepthRT;
    };

    auto linearize_depth_pass = m_pRenderGraph->AddPass<LinearizeDepthPassData>("Linearize Depth", RenderPassType::Compute,
        [&](LinearizeDepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::R32F;
            data.outputLinearDepthRT = builder.Create<RenderGraphTexture>(desc, "LinearDepth RT");

            data.inputDepthRT = builder.Read(depth);
            data.outputLinearDepthRT = builder.Write(data.outputLinearDepthRT);
        },
        [&](const LinearizeDepthPassData& data, IGfxCommandList* pCommandList)
        {
            GfxComputePipelineDesc psoDesc;
            psoDesc.cs = GetShader("linearize_depth.hlsl", "main", "cs_6_6", {});
            IGfxPipelineState* pPSO = GetPipelineState(psoDesc, "LinearizeDepth PSO");

            pCommandList->SetPipelineState(pPSO);

            RenderGraphTexture* inputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.inputDepthRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.outputLinearDepthRT);
            uint32_t cb[2] = { inputRT->GetSRV()->GetHeapIndex(), outputRT->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));

            pCommandList->Dispatch((m_nWindowWidth + 7) / 8, (m_nWindowHeight + 7) / 8, 1);
        });

    return linearize_depth_pass->outputLinearDepthRT;
}

void Renderer::ObjectIDPass(RenderGraphHandle& depth)
{
    if (m_bEnableObjectIDRendering)
    {
        struct IDPassData
        {
            RenderGraphHandle idTexture;
            RenderGraphHandle sceneDepthTexture;
        };

        auto id_pass = m_pRenderGraph->AddPass<IDPassData>("Object ID Pass", RenderPassType::Graphics,
            [&](IDPassData& data, RenderGraphBuilder& builder)
            {
                RenderGraphTexture::Desc desc;
                desc.width = m_nWindowWidth;
                desc.height = m_nWindowHeight;
                desc.format = GfxFormat::R32UI;
                data.idTexture = builder.Create<RenderGraphTexture>(desc, "Object ID");

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
            RenderGraphHandle srcTexture;
        };

        m_pRenderGraph->AddPass<CopyIDPassData>("Copy ID to Readback Buffer", RenderPassType::Copy,
            [&](CopyIDPassData& data, RenderGraphBuilder& builder)
            {
                data.srcTexture = builder.Read(id_pass->idTexture);

                builder.MakeTarget();
            },
            [&](const CopyIDPassData& data, IGfxCommandList* pCommandList)
            {
                RenderGraphTexture* srcTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcTexture);

                m_nObjectIDRowPitch = srcTexture->GetTexture()->GetRowPitch(0);
                uint32_t size = m_nObjectIDRowPitch * srcTexture->GetTexture()->GetDesc().height;
                if (m_pObjectIDBuffer == nullptr || m_pObjectIDBuffer->GetDesc().size < size)
                {
                    GfxBufferDesc desc;
                    desc.size = size;
                    desc.memory_type = GfxMemoryType::GpuToCpu;
                    m_pObjectIDBuffer.reset(m_pDevice->CreateBuffer(desc, "Renderer::m_pObjectIDBuffer"));
                }

                pCommandList->CopyTextureToBuffer(m_pObjectIDBuffer.get(), srcTexture->GetTexture(), 0, 0);
            });
    }
}

void Renderer::CopyHistoryPass(RenderGraphHandle linearDepth, RenderGraphHandle normal, RenderGraphHandle sceneColor)
{
    struct CopyPassData
    {
        RenderGraphHandle srcLinearDepthTexture;
        RenderGraphHandle dstLinearDepthTexture;

        RenderGraphHandle srcNormalTexture;
        RenderGraphHandle dstNormalTexture;

        RenderGraphHandle srcSceneColorTexture;
        RenderGraphHandle dstSceneColorTexture;
    };

    m_pRenderGraph->AddPass<CopyPassData>("Copy History Textures", RenderPassType::Copy,
        [&](CopyPassData& data, RenderGraphBuilder& builder)
        {
            data.srcLinearDepthTexture = builder.Read(linearDepth);
            data.dstLinearDepthTexture = builder.Write(m_prevLinearDepthHandle);

            data.srcNormalTexture = builder.Read(normal);
            data.dstNormalTexture = builder.Write(m_prevNormalHandle);

            data.srcSceneColorTexture = builder.Read(sceneColor);
            data.dstSceneColorTexture = builder.Write(m_prevSceneColorHandle);

            builder.MakeTarget();
        },
        [&](const CopyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* srcLinearDepthTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcLinearDepthTexture);
            RenderGraphTexture* srcNormalTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcNormalTexture);
            RenderGraphTexture* srcSceneColorTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcSceneColorTexture);

            pCommandList->CopyTexture(m_pPrevLinearDepthTexture->GetTexture(), 0, 0, srcLinearDepthTexture->GetTexture(), 0, 0);
            pCommandList->CopyTexture(m_pPrevNormalTexture->GetTexture(), 0, 0, srcNormalTexture->GetTexture(), 0, 0);
            pCommandList->CopyTexture(m_pPrevSceneColorTexture->GetTexture(), 0, 0, srcSceneColorTexture->GetTexture(), 0, 0);
        });
}
