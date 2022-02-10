#include "taa.h"
#include "../renderer.h"
#include "core/engine.h"

#include "taa_constants.hlsli"

struct TAAVelocityPassData
{
    RenderGraphHandle inputDepthRT;
    RenderGraphHandle inputVelocityRT;
    RenderGraphHandle outputMotionVectorRT;
};

struct TAAPassData
{
    RenderGraphHandle inputRT;
    RenderGraphHandle historyRT;
    RenderGraphHandle velocityRT;
    RenderGraphHandle linearDepthRT;
    RenderGraphHandle prevLinearDepthRT;
    RenderGraphHandle outputRT;
};

struct TAAApplyPassData
{
    RenderGraphHandle inputRT;
    RenderGraphHandle outputRT;
    RenderGraphHandle outputHistoryRT;
};

TAA::TAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    std::vector<std::string> defines;
    defines.push_back("USE_YCOCG_SPACE=0");
    defines.push_back("LONGEST_VELOCITY_VECTOR_SAMPLES=0");

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "main", "cs_6_6", defines);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "TAA PSO");

    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "apply_main", "cs_6_6", {});
    m_pApplyPSO = pRenderer->GetPipelineState(psoDesc, "TAA apply PSO");

    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "generate_velocity_main", "cs_6_6", {});
    m_pMotionVectorPSO = pRenderer->GetPipelineState(psoDesc, "TAA motion vector PSO");
}

RenderGraphHandle TAA::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, RenderGraphHandle sceneDepthRT,
    RenderGraphHandle linearDepthRT, RenderGraphHandle velocityRT, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "TAA");

    auto taa_velocity_pass = pRenderGraph->AddPass<TAAVelocityPassData>("TAA generate velocity",
        [&](TAAVelocityPassData& data, RenderGraphBuilder& builder)
        {
            data.inputDepthRT = builder.Read(sceneDepthRT, GfxResourceState::ShaderResourceNonPS);
            data.inputVelocityRT = builder.Read(velocityRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputMotionVectorRT = builder.Create<RenderGraphTexture>(desc, "TAA motion vector");
            data.outputMotionVectorRT = builder.Write(data.outputMotionVectorRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputDepthRT);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputVelocityRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputMotionVectorRT);

            GenerateMotionVector(pCommandList, depthRT->GetSRV(), velocityRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    auto taa_pass = pRenderGraph->AddPass<TAAPassData>("TAA",
        [&](TAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphHandle historyRT = builder.Import(GetHistoryRT(width, height), GfxResourceState::UnorderedAccess);

            data.inputRT = builder.Read(sceneColorRT, GfxResourceState::ShaderResourceNonPS);
            data.historyRT = builder.Read(historyRT, GfxResourceState::ShaderResourceNonPS);
            data.velocityRT = builder.Read(taa_velocity_pass->outputMotionVectorRT, GfxResourceState::ShaderResourceNonPS);
            data.linearDepthRT = builder.Read(linearDepthRT, GfxResourceState::ShaderResourceNonPS);
            data.prevLinearDepthRT = builder.Read(m_pRenderer->GetPrevLinearDepthHandle(), GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Output");
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocityRT);
            RenderGraphTexture* linearDepthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.linearDepthRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRT);

            Draw(pCommandList, inputRT->GetSRV(), velocityRT->GetSRV(), linearDepthRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    auto taa_apply_pass = pRenderGraph->AddPass<TAAApplyPassData>("TAA apply",
        [&](TAAApplyPassData& data, RenderGraphBuilder& builder)
        {
            data.inputRT = builder.Read(taa_pass->outputRT, GfxResourceState::ShaderResourceNonPS);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "TAA Final");
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::UnorderedAccess);

            data.outputHistoryRT = builder.Write(taa_pass->historyRT, GfxResourceState::UnorderedAccess);
        },
        [=](const TAAApplyPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inputRT);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outputRT);

            Apply(pCommandList, inputRT->GetSRV(), outputRT->GetUAV(), width, height);
        });

    return taa_apply_pass->outputRT;
}

IGfxTexture* TAA::GetHistoryRT(uint32_t width, uint32_t height)
{
    if (m_pHistoryColor == nullptr ||
        m_pHistoryColor->GetTexture()->GetDesc().width != width ||
        m_pHistoryColor->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryColor.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "TAA HistoryTexture"));
        m_bHistoryInvalid = true;
    }

    return m_pHistoryColor->GetTexture();
}

void TAA::GenerateMotionVector(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pMotionVectorPSO);

    uint32_t cb[3] = { depth->GetHeapIndex(), velocity->GetHeapIndex(), output->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void TAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* velocity, IGfxDescriptor* linearDepth, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    float2 jitterDelta = camera->GetPrevJitter() - camera->GetJitter();

    IGfxDescriptor* history = m_pHistoryColor->GetSRV();
    if (m_bHistoryInvalid)
    {
        history = input;
        m_bHistoryInvalid = false;
    }

    TAAConstant cb = {};
    cb.velocityRT = velocity->GetHeapIndex();
    cb.colorRT = input->GetHeapIndex();
    cb.historyRT = history->GetHeapIndex();
    cb.depthRT = linearDepth->GetHeapIndex();
    if (m_pRenderer->GetPrevLinearDepthTexture())
    {
        cb.prevDepthRT = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    }
    else
    {
        cb.prevDepthRT = cb.depthRT;
    }
    cb.outTexture = output->GetHeapIndex();
    cb.consts.Resolution = { (float)width, (float)height, 1.0f / (float)width, 1.0f / (float)height };
    cb.consts.Jitter = { jitterDelta.x, jitterDelta.y };
    cb.consts.FrameNumber = m_pRenderer->GetFrameID() % 2;
    cb.consts.DebugFlags = 0;
    cb.consts.LerpMul = 0.99f;
    cb.consts.LerpPow = 1.0f;
    cb.consts.VarClipGammaMin = 0.75f;
    cb.consts.VarClipGammaMax = 6.0f;
    cb.consts.PreExposureNewOverOld = 1.0f;

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void TAA::Apply(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pApplyPSO);

    uint32_t cb[3] = { input->GetHeapIndex(), output->GetHeapIndex(), m_pHistoryColor->GetUAV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
