#include "cas.h"
#include "../renderer.h"

// CAS
#define A_CPU
#include "ffx_a.h"
#include "ffx_cas.h"

CAS::CAS(Renderer* pRenderer)
{
	m_pRenderer = pRenderer;

	GfxComputePipelineDesc psoDesc;
	psoDesc.cs = pRenderer->GetShader("cas.hlsl", "main", "cs_6_6", {});
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "CAS PSO");
}

void CAS::Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height)
{
	pCommandList->SetPipelineState(m_pPSO);

	struct CASConstants
	{
		uint input;
		uint output;
		uint2 __padding;

		uint4 const0;
		uint4 const1;
	};

	CASConstants consts;
	consts.input = input->GetHeapIndex();
	consts.output = output->GetHeapIndex();

	CasSetup(reinterpret_cast<AU1*>(&consts.const0), reinterpret_cast<AU1*>(&consts.const1), m_sharpenVal, (AF1)width, (AF1)height, (AF1)width, (AF1)height);

	pCommandList->SetComputeConstants(1, &consts, sizeof(consts));

	// This value is the image region dim that each thread group of the CAS shader operates on
	static const int threadGroupWorkRegionDim = 16;
	int dispatchX = (width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	int dispatchY = (height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

	pCommandList->Dispatch(dispatchX, dispatchY, 1);
}
