#include "fxaa.h"
#include "renderer.h"

FXAA::FXAA(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

	GfxComputePipelineDesc psoDesc;
	psoDesc.cs = pRenderer->GetShader("fxaa.hlsl", "cs_main", "cs_6_6", {});
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "FXAA PSO");
}

void FXAA::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
	pCommandList->SetPipelineState(m_pPSO);

	struct CB
	{
		uint c_ldrTexture;
		uint c_outTexture;
		uint c_linearSampler;
		uint c_width;
		uint c_height;
		float c_rcpWidth;
		float c_rcpHeight;
	};

	CB constantBuffer = {
		input->GetHeapIndex(), 
		output->GetHeapIndex(),
		m_pRenderer->GetLinearSampler()->GetHeapIndex(),
		width,
		height,
		1.0f / width,
		1.0f / height,
	};

	pCommandList->SetComputeConstants(1, &constantBuffer, sizeof(constantBuffer));

	pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
