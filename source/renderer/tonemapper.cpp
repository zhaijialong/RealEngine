#include "tonemapper.h"
#include "renderer.h"

Tonemapper::Tonemapper(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("tone_mapping.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("tone_mapping.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "ToneMapping PSO");
}

void Tonemapper::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrRT)
{
	pCommandList->SetPipelineState(m_pPSO);

	uint32_t resourceCB[4] = { pHdrRT->GetHeapIndex(), m_pRenderer->GetPointSampler()->GetHeapIndex(), 0, 0 };
	pCommandList->SetGraphicsConstants(0, resourceCB, sizeof(resourceCB));

	pCommandList->Draw(3);
}
