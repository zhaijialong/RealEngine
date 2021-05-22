#include "tonemap.h"
#include "renderer.h"

Tonemap::Tonemap(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("tonemap.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("tonemap.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "Tonemap PSO");
}

void Tonemap::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* pHdrRT)
{
    RENDER_EVENT(pCommandList, "Tonemap");

	pCommandList->SetPipelineState(m_pPSO);

	uint32_t resourceCB[4] = { pHdrRT->GetHeapIndex(), m_pRenderer->GetPointSampler()->GetHeapIndex(), 0, 0 };
	pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, resourceCB, sizeof(resourceCB));

	pCommandList->Draw(3);
}
