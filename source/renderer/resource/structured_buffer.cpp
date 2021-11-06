#include "structured_buffer.h"
#include "core/engine.h"
#include "../renderer.h"

StructuredBuffer::StructuredBuffer(const std::string& name)
{
    m_name = name;
}

bool StructuredBuffer::Create(uint32_t stride, uint32_t element_count, GfxMemoryType memory_type, bool uav)
{
	Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
	IGfxDevice* pDevice = pRenderer->GetDevice();

	GfxBufferDesc desc;
	desc.stride = stride;
	desc.size = stride * element_count;
	desc.format = GfxFormat::Unknown;
	desc.memory_type = memory_type;
	desc.usage = GfxBufferUsageStructuredBuffer;

	if (uav)
	{
		desc.usage |= GfxBufferUsageUnorderedAccess;
	}

	m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
	if (m_pBuffer == nullptr)
	{
		return false;
	}

	GfxShaderResourceViewDesc srvDesc;
	srvDesc.type = GfxShaderResourceViewType::StructuredBuffer;
	srvDesc.buffer.size = stride * element_count;
	m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
	if (m_pSRV == nullptr)
	{
		return false;
	}

	if (uav)
	{
		GfxUnorderedAccessViewDesc uavDesc;
		uavDesc.type = GfxUnorderedAccessViewType::StructuredBuffer;
		uavDesc.buffer.size = stride * element_count;
		m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pBuffer.get(), uavDesc, m_name));
		if (m_pUAV == nullptr)
		{
			return false;
		}
	}

	return true;
}
