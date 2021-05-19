#include "sky_sphere.h"
#include "engine.h"

bool SkySphere::Create()
{
	const float M_PI = 3.141592653f;

	std::vector<float3> vertices;
	std::vector<uint16_t> indices;

	int latitudeBands = 100;
	int longitudeBands = 100;
	float radius = 1000.0f;

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

	m_nIndexCount = (uint32_t)indices.size();

	GfxBufferDesc desc;
	desc.stride = sizeof(uint16_t);
	desc.size = sizeof(uint16_t) * m_nIndexCount;
	desc.format = GfxFormat::R16UI;

	m_pIndexBuffer.reset(pDevice->CreateBuffer(desc, "SkySphere IB"));
	if (m_pIndexBuffer == nullptr)
	{
		return false;
	}

	desc.stride = sizeof(float3);
	desc.size = sizeof(float3) * (uint32_t)vertices.size();
	desc.format = GfxFormat::Unknown;
	desc.usage = GfxBufferUsageStructuredBuffer;
	m_pVertexBuffer.reset(pDevice->CreateBuffer(desc, "SkySphere VB"));
	if (m_pVertexBuffer == nullptr)
	{
		return false;
	}

	pRenderer->UploadBuffer(m_pIndexBuffer.get(), indices.data(), sizeof(uint16_t) * (uint32_t)indices.size());
	pRenderer->UploadBuffer(m_pVertexBuffer.get(), vertices.data(), sizeof(float3) * (uint32_t)vertices.size());

	GfxShaderResourceViewDesc srvDesc;
	srvDesc.type = GfxShaderResourceViewType::StructuredBuffer;
	srvDesc.buffer.size = sizeof(float3) * (uint32_t)vertices.size();
	m_pVertexBufferSRV.reset(pDevice->CreateShaderResourceView(m_pVertexBuffer.get(), srvDesc, "SkySphere VB SRV"));

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("sky_sphere.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("sky_sphere.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.depthstencil_state.depth_write = false;
	psoDesc.depthstencil_state.depth_test = true;
	psoDesc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
	psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
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
	pRenderer->AddBasePassBatch(bassPassBatch);
}

void SkySphere::RenderSky(IGfxCommandList* pCommandList, Renderer* pRenderer, Camera* pCamera)
{
	pCommandList->SetPipelineState(m_pPSO);
	pCommandList->SetIndexBuffer(m_pIndexBuffer.get());

	struct SkySphereConstant
	{
		float4x4 mtxWVP;
		float4x4 mtxWorld;
		float3 cameraPos;
		uint posBuffer;
	};

	float4x4 mtxWorld = translation_matrix(pCamera->GetPosition());

	SkySphereConstant CB;
	CB.mtxWVP = mul(pCamera->GetViewProjectionMatrix(), mtxWorld);
	CB.mtxWorld = mtxWorld;
	CB.cameraPos = pCamera->GetPosition();
	CB.posBuffer = m_pVertexBufferSRV->GetHeapIndex();
	
	pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 1, &CB, sizeof(CB));

	pCommandList->DrawIndexed(m_nIndexCount, 1);
}
