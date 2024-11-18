#include "mock_buffer.h"
#include "mock_device.h"
#include "utils/memory.h"

MockBuffer::MockBuffer(MockDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockBuffer::~MockBuffer()
{
    if (m_pCpuAddress)
    {
        RE_FREE(m_pCpuAddress);
    }
}

bool MockBuffer::Create()
{
    if (m_desc.memory_type != GfxMemoryType::GpuOnly)
    {
        m_pCpuAddress = RE_ALLOC(m_desc.size);
        memset(m_pCpuAddress, 0, m_desc.size);
    }

    return true;
}

void* MockBuffer::GetHandle() const
{
    return nullptr;
}

void* MockBuffer::GetCpuAddress()
{
    return m_pCpuAddress;
}

uint64_t MockBuffer::GetGpuAddress()
{
    return 0;
}

uint32_t MockBuffer::GetRequiredStagingBufferSize() const
{
    return m_desc.size;
}

void* MockBuffer::GetSharedHandle() const
{
    return nullptr;
}
