#include "sky_sphere.h"
#include "core/engine.h"

bool SkySphere::Create()
{
    const float PI = 3.141592653f;

    eastl::vector<float3> vertices;
    eastl::vector<uint16_t> indices;

    int latitudeBands = 50;
    int longitudeBands = 50;
    float radius = 3000.0f;

    for (int latNumber = 0; latNumber <= latitudeBands; latNumber++)
    {
        float theta = latNumber * PI / latitudeBands;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int longNumber = 0; longNumber <= longitudeBands; longNumber++)
        {
            float phi = longNumber * 2 * PI / longitudeBands;
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
    psoDesc.depthstencil_format = GfxFormat::D32F;
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "SkySphere PSO");

    return true;
}

void SkySphere::Tick(float delta_time)
{
}

void SkySphere::Render(Renderer* pRenderer)
{
    RenderBatch& batch = pRenderer->AddForwardPassBatch();
    batch.label = "SkySphere";

    batch.SetPipelineState(m_pPSO);
    batch.SetIndexBuffer(m_pIndexBuffer->GetBuffer(), 0, m_pIndexBuffer->GetFormat());

    uint32_t posBuffer = m_pVertexBuffer->GetSRV()->GetHeapIndex();
    batch.SetConstantBuffer(0, &posBuffer, sizeof(posBuffer));

    batch.DrawIndexed(m_pIndexBuffer->GetIndexCount());
}
