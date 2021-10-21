#include "raw_buffer.h"
#include "core/engine.h"
#include "../renderer.h"

RawBuffer::RawBuffer(const std::string& name)
{
    m_name = name;
}

bool RawBuffer::Create(uint32_t size, GfxMemoryType memory_type)
{
	Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
	IGfxDevice* pDevice = pRenderer->GetDevice();

	RE_ASSERT(size % 4 == 0);

	GfxBufferDesc desc;
	desc.stride = 4;
	desc.size = size;
	desc.format = GfxFormat::R32F;
	desc.memory_type = memory_type;
	desc.usage = GfxBufferUsageRawBuffer;

	m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
	if (m_pBuffer == nullptr)
	{
		return false;
	}

	GfxShaderResourceViewDesc srvDesc;
	srvDesc.type = GfxShaderResourceViewType::RawBuffer;
	srvDesc.buffer.size = size;
	m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
	if (m_pSRV == nullptr)
	{
		return false;
	}

	return true;
}
