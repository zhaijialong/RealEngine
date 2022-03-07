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
    RenderGraphHandle sceneColorRT;

    if (m_outputType == RendererOutput::PathTracing)
    {
        sceneColorRT = m_pPathTracer->Render(m_pRenderGraph.get(), m_nWindowWidth, m_nWindowHeight);
    }
    else
    {
        sceneColorRT = m_pLightingProcessor->Render(m_pRenderGraph.get(), m_nWindowWidth, m_nWindowHeight);
    }

    ForwardPass(sceneColorRT, sceneDepthRT);

    RenderGraphHandle velocityRT = VelocityPass(sceneDepthRT);
    RenderGraphHandle linearDepthRT = LinearizeDepthPass(sceneDepthRT);

    RenderGraphHandle output = m_pPostProcessor->Render(m_pRenderGraph.get(), sceneColorRT, sceneDepthRT, linearDepthRT, velocityRT, m_nWindowWidth, m_nWindowHeight);

    ObjectIDPass(sceneDepthRT);
    CopyLinearDepthPass(linearDepthRT);

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

    auto forward_pass = m_pRenderGraph->AddPass<ForwardPassData>("Forward Pass",
        [&](ForwardPassData& data, RenderGraphBuilder& builder)
        {
            data.outSceneColorRT = builder.WriteColor(0, color, 0, GfxRenderPassLoadOp::Load);
            data.outSceneDepthRT = builder.WriteDepth(depth, 0, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load);
        },
        [&](const ForwardPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_forwardPassBatchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_forwardPassBatchs[i]);
            }
            m_forwardPassBatchs.clear();
        });

    color = forward_pass->outSceneColorRT;
    depth = forward_pass->outSceneDepthRT;
}

RenderGraphHandle Renderer::VelocityPass(RenderGraphHandle& depth)
{
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
            desc.usage = GfxTextureUsageRenderTarget;
            desc.format = GfxFormat::RGBA16F;
            data.outVelocityRT = builder.Create<RenderGraphTexture>(desc, "Velocity RT");

            data.outVelocityRT = builder.WriteColor(0, data.outVelocityRT, 0, GfxRenderPassLoadOp::Clear, float4(0.0));
            data.outSceneDepthRT = builder.ReadDepth(depth, 0);
        },
        [&](const VelocityPassData& data, IGfxCommandList* pCommandList)
        {
            for (size_t i = 0; i < m_velocityPassBatchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_velocityPassBatchs[i]);
            }
            m_velocityPassBatchs.clear();
        });

    depth = velocity_pass->outSceneDepthRT;
    return velocity_pass->outVelocityRT;
}

RenderGraphHandle Renderer::LinearizeDepthPass(RenderGraphHandle depth)
{
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
            desc.usage = GfxTextureUsageUnorderedAccess;
            desc.format = GfxFormat::R32F;
            data.outputLinearDepthRT = builder.Create<RenderGraphTexture>(desc, "LinearDepth RT");

            data.inputDepthRT = builder.Read(depth, GfxResourceState::ShaderResourceNonPS);
            data.outputLinearDepthRT = builder.Write(data.outputLinearDepthRT, GfxResourceState::UnorderedAccess);
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

        auto id_pass = m_pRenderGraph->AddPass<IDPassData>("Object ID Pass",
            [&](IDPassData& data, RenderGraphBuilder& builder)
            {
                RenderGraphTexture::Desc desc;
                desc.width = m_nWindowWidth;
                desc.height = m_nWindowHeight;
                desc.usage = GfxTextureUsageRenderTarget;
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
                m_idPassBatchs.clear();
            });

        depth = id_pass->sceneDepthTexture;

        struct CopyIDPassData
        {
            RenderGraphHandle srcTexture;
        };

        m_pRenderGraph->AddPass<CopyIDPassData>("Copy ID to Readback Buffer",
            [&](CopyIDPassData& data, RenderGraphBuilder& builder)
            {
                data.srcTexture = builder.Read(id_pass->idTexture, GfxResourceState::CopySrc);

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
    else
    {
        m_idPassBatchs.clear();
    }
}

void Renderer::CopyLinearDepthPass(RenderGraphHandle linearDepth)
{
    struct CopyPassData
    {
        RenderGraphHandle srcTexture;
        RenderGraphHandle dstTexture;
    };

    m_pRenderGraph->AddPass<CopyPassData>("Copy Depth",
        [&](CopyPassData& data, RenderGraphBuilder& builder)
        {
            data.srcTexture = builder.Read(linearDepth, GfxResourceState::CopySrc);
            data.dstTexture = builder.Write(m_prevLinearDepthHandle, GfxResourceState::CopyDst);

            builder.MakeTarget();
        },
        [&](const CopyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* srcTexture = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.srcTexture);

            pCommandList->CopyTexture(m_pPrevLinearDepthTexture->GetTexture(), 0, 0, srcTexture->GetTexture(), 0, 0);
        });
}
