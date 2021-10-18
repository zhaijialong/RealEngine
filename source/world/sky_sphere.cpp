#include "sky_sphere.h"
#include "core/engine.h"

bool SkySphere::Create()
{
	const float M_PI = 3.141592653f;

	std::vector<float3> vertices;
	std::vector<uint16_t> indices;

	int latitudeBands = 50;
	int longitudeBands = 50;
	float radius = 3000.0f;

	for (int latNumber = 0; latNumber <= latitudeBands; latNumber++)
	{
		float theta = latNumber * M_PI / latitudeBands;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);

		for (int longNumber = 0; longNumber <= longitudeBands; longNumber++)
		{
			float phi = longNumber * 2 * M_PI / longitudeBands;
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			float3 vs;
			vs.x = radius * cosPhi * sinTheta;
			vs.y = radius * cosTheta;
			vs.z = radius * sinPhi * sinTheta;

			vertices.push_back(vs);

			uint16_t first = (latNumber * (longitudeBands + 1)) + longNumber;
			uint16_t second = first + longitudeBands + 1;

			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}

	Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
	IGfxDevice* pDevice = pRenderer->GetDevice();

	m_pIndexBuffer.reset(pRenderer->CreateIndexBuffer(indices.data(), sizeof(uint16_t), (uint32_t)indices.size(), "SkySphere IB"));
	if (m_pIndexBuffer == nullptr)
	{
		return false;
	}

	m_pVertexBuffer.reset(pRenderer->CreateStructuredBuffer(vertices.data(), sizeof(float3), (uint32_t)vertices.size(), "SkySphere VB"));
	if (m_pVertexBuffer == nullptr)
	{
		return false;
	}

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("sky_sphere.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("sky_sphere.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.depthstencil_state.depth_write = false;
	psoDesc.depthstencil_state.depth_test = true;
	psoDesc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
	psoDesc.rt_format[0] = GfxFormat::RGBA16F;
	psoDesc.depthstencil_format = GfxFormat::D32FS8;
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "SkySphere PSO");

    return true;
}

void SkySphere::Tick(float delta_time)
{
}

void SkySphere::Render(Renderer* pRenderer)
{
	RenderFunc bassPassBatch = std::bind(&SkySphere::RenderSky, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	pRenderer->AddForwardPassBatch(bassPassBatch);
}

void SkySphere::RenderSky(IGfxCommandList* pCommandList, Renderer* pRenderer, Camera* pCamera)
{
	GPU_EVENT(pCommandList, "SkySphere");

	pCommandList->SetPipelineState(m_pPSO);
	pCommandList->SetIndexBuffer(m_pIndexBuffer->GetBuffer());

	struct SkySphereConstant
	{
		float4x4 mtxWVP;
		float4x4 mtxWorld;
		uint posBuffer;
		float3 cameraPos;
	};

	float4x4 mtxWorld = translation_matrix(pCamera->GetPosition());

	SkySphereConstant CB;
	CB.mtxWVP = mul(pCamera->GetViewProjectionMatrix(), mtxWorld);
	CB.mtxWorld = mtxWorld;
	CB.cameraPos = pCamera->GetPosition();
	CB.posBuffer = m_pVertexBuffer->GetSRV()->GetHeapIndex();
	
	pCommandList->SetGraphicsConstants(1, &CB, sizeof(CB));

	pCommandList->DrawIndexed(m_pIndexBuffer->GetIndexCount());
}
