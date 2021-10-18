#include "clustered_shading.h"
#include "../renderer.h"
#include "core/engine.h"

float2 DepthlinearizationParams(const float4x4& ProjMatrix)
{
	// The perspective depth projection comes from the the following projection matrix:
	//
	// | 1  0  0  0 |
	// | 0  1  0  0 |
	// | 0  0  A  1 |
	// | 0  0  B  0 |
	//
	// Z' = (Z * A + B) / Z
	// Z' = A + B / Z
	//
	// So to get Z from Z' is just:
	// Z = B / (Z' - A)
	//
	// Note a reversed Z projection matrix will have A=0.
	//
	// Done in shader as:
	// Z = 1 / (Z' * C1 - C2)   --- Where C1 = 1/B, C2 = A/B
	//

	float DepthMul = ProjMatrix[2][2];
	float DepthAdd = ProjMatrix[3][2];

	if (DepthAdd == 0.f)
	{
		// Avoid dividing by 0 in this case
		DepthAdd = 0.00000001f;
	}

    float SubtractValue = DepthMul / DepthAdd;

	// Subtract a tiny number to avoid divide by 0 errors in the shader when a very far distance is decided from the depth buffer.
	// This fixes fog not being applied to the black background in the editor.
	SubtractValue -= 0.00000001f;

	return float2(
		1.0f / DepthAdd,
		SubtractValue
	);
}

ClusteredShading::ClusteredShading(Renderer* pRenderer)
{
	m_pRenderer = pRenderer;

	GfxComputePipelineDesc psoDesc;
	psoDesc.cs = pRenderer->GetShader("clustered_shading.hlsl", "main", "cs_6_6", {});
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "ClusteredShading PSO");
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList, const ClusterShadingPassData& data, uint32_t width, uint32_t height)
{
	RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();

	RenderGraphTexture* albedoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inAlbedoRT);
	RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inNormalRT);
	RenderGraphTexture* emissiveRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inEmissiveRT);
	RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inDepthRT);
	RenderGraphTexture* shadowRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inShadowRT);
	RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outHdrRT);

	pCommandList->SetPipelineState(m_pPSO);

	struct CB0
	{
		uint width;
		uint height;
		float invWidth;
		float invHeight;
	};

	CB0 cb0 = { width, height, 1.0f / width, 1.0f / height };
	pCommandList->SetComputeConstants(0, &cb0, sizeof(cb0));

	World* world = Engine::GetInstance()->GetWorld();
	Camera* camera = world->GetCamera();
	float4x4 mtxProj = camera->GetProjectionMatrix();
	float4x4 mtxViewProj = camera->GetViewProjectionMatrix();

	struct CB1
	{
		uint albedoRT;
		uint normalRT;
		uint emissiveRT;
		uint depthRT;

		uint shadowRT;
		uint hdrRT;
		float2 zparams;

		float4x4 mtxInvViewProj;
	};

	CB1 cb1;
	cb1.albedoRT = albedoRT->GetSRV()->GetHeapIndex();
	cb1.normalRT = normalRT->GetSRV()->GetHeapIndex();
	cb1.emissiveRT = emissiveRT->GetSRV()->GetHeapIndex();
	cb1.depthRT = depthRT->GetSRV()->GetHeapIndex();
	cb1.shadowRT = shadowRT->GetSRV()->GetHeapIndex();
	cb1.hdrRT = hdrRT->GetUAV()->GetHeapIndex();
	cb1.zparams = DepthlinearizationParams(mtxProj);
	cb1.mtxInvViewProj = inverse(mtxViewProj);

	pCommandList->SetComputeConstants(1, &cb1, sizeof(cb1));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
