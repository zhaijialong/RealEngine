#include "taa.h"
#include "../renderer.h"
#include "core/engine.h"

#include "taa_constants.hlsli"

TAA::TAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    std::vector<std::string> defines;
    //defines.push_back("USE_DEPTH_THRESHOLD=0");

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "main", "cs_6_6", defines);
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "TAA PSO");

    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "apply_main", "cs_6_6", {});
    m_pApplyPSO = pRenderer->GetPipelineState(psoDesc, "TAA apply PSO");

    psoDesc.cs = pRenderer->GetShader("taa.hlsl", "generate_velocity_main", "cs_6_6", {});
    m_pMotionVectorPSO = pRenderer->GetPipelineState(psoDesc, "TAA motion vector PSO");
}

IGfxTexture* TAA::GetHistoryRT(uint32_t width, uint32_t height)
{
    if (m_pHistoryColor == nullptr ||
        m_pHistoryColor->GetTexture()->GetDesc().width != width ||
        m_pHistoryColor->GetTexture()->GetDesc().height != height)
    {
        m_pHistoryColor.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess | GfxTextureUsageShaderResource, "TAA HistoryTexture"));
    }

    return m_pHistoryColor->GetTexture();
}

void TAA::GenerateMotionVector(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pMotionVectorPSO);

    uint32_t cb[4] = { depth->GetHeapIndex(), velocity->GetHeapIndex(), output->GetHeapIndex(), 0 };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void TAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* velocity, IGfxDescriptor* linearDepth, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPSO);

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    float2 jitterDelta = camera->GetPrevJitter() - camera->GetJitter();

    TAAConstant cb = {};
    cb.velocityRT = velocity->GetHeapIndex();
    cb.colorRT = input->GetHeapIndex();
    cb.historyRT = m_pHistoryColor->GetSRV()->GetHeapIndex();
    cb.depthRT = linearDepth->GetHeapIndex();
    if (m_pRenderer->GetPrevLinearDepthTexture())
    {
        cb.prevDepthRT = m_pRenderer->GetPrevLinearDepthTexture()->GetSRV()->GetHeapIndex();
    }
    else
    {
        cb.prevDepthRT = cb.historyRT;
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

    uint32_t cb[4] = { input->GetHeapIndex(), output->GetHeapIndex(), m_pHistoryColor->GetUAV()->GetHeapIndex(), 0 };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
