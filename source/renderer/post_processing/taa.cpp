#include "taa.h"
#include "../renderer.h"

TAA::TAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

	GfxComputePipelineDesc psoDesc;
	psoDesc.cs = pRenderer->GetShader("taa.hlsl", "main", "cs_6_6", {});
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "TAA PSO");
}

void TAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
	if (m_pHistoryColor == nullptr ||
		m_pHistoryColor->GetTexture()->GetDesc().width != width ||
		m_pHistoryColor->GetTexture()->GetDesc().height != height)
	{
		m_pHistoryColor.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageShaderResource, "TAA::m_pHistoryColor"));
		//todo : clear
	}

	pCommandList->SetPipelineState(m_pPSO);

	uint32_t cb[5] = { input->GetHeapIndex(), m_pHistoryColor->GetSRV()->GetHeapIndex(), output->GetHeapIndex(), width, height };
	pCommandList->SetComputeConstants(1, cb, sizeof(cb));

	pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void TAA::CopyHistory(IGfxCommandList* pCommandList, IGfxTexture* texture)
{
	pCommandList->ResourceBarrier(m_pHistoryColor->GetTexture(), 0, GfxResourceState::ShaderResourceNonPS, GfxResourceState::CopyDst);
	pCommandList->CopyTexture(m_pHistoryColor->GetTexture(), texture);
	pCommandList->ResourceBarrier(m_pHistoryColor->GetTexture(), 0, GfxResourceState::CopyDst, GfxResourceState::ShaderResourceNonPS);
}
