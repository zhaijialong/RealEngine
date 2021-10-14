#include "clustered_shading.h"
#include "renderer.h"

ClusteredShading::ClusteredShading(Renderer* pRenderer)
{
	m_pRenderer = pRenderer;

	GfxComputePipelineDesc psoDesc;
	psoDesc.cs = pRenderer->GetShader("clustered_shading.hlsl", "main", "cs_6_6", {});
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "ClusteredShading PSO");
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList)
{
	pCommandList->SetPipelineState(m_pPSO);
    pCommandList->Dispatch(1, 1, 1);
}
