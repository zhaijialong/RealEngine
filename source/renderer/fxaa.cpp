#include "fxaa.h"
#include "renderer.h"

FXAA::FXAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("fxaa.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("fxaa.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "FXAA PSO");
}

void FXAA::Draw(IGfxCommandList* pCommandList, Texture2D* pLdrRT)
{
	RENDER_EVENT(pCommandList, "FXAA");

	pCommandList->SetPipelineState(m_pPSO);

	struct CB
	{
		uint c_ldrTexture;
		uint c_linearSampler;
		float c_rcpWidth;
		float c_rcpHeight;
	};

	CB constantBuffer = {
		pLdrRT->GetSRV()->GetHeapIndex(), 
		m_pRenderer->GetLinearSampler()->GetHeapIndex(), 
		1.0f / pLdrRT->GetTexture()->GetDesc().width,
		1.0f / pLdrRT->GetTexture()->GetDesc().height,
	};

	pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, &constantBuffer, sizeof(constantBuffer));

	pCommandList->Draw(3);
}
