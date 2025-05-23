#include "webgpu_buffer.h"
#include "webgpu_device.h"
#include "utils/memory.h"

WebGPUBuffer::WebGPUBuffer(WebGPUDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPUBuffer::~WebGPUBuffer()
{
    if (m_pCpuAddress)
    {
        RE_FREE(m_pCpuAddress);
    }
}

bool WebGPUBuffer::Create()
{
    if (m_desc.memory_type != GfxMemoryType::GpuOnly)
    {
        m_pCpuAddress = RE_ALLOC(m_desc.size);
        memset(m_pCpuAddress, 0, m_desc.size);
    }

    return true;
}

void* WebGPUBuffer::GetHandle() const
{
    return nullptr;
}

void* WebGPUBuffer::GetCpuAddress()
{
    return m_pCpuAddress;
}

uint64_t WebGPUBuffer::GetGpuAddress()
{
    return 0;
}

uint32_t WebGPUBuffer::GetRequiredStagingBufferSize() const
{
    return m_desc.size;
}

void* WebGPUBuffer::GetSharedHandle() const
{
    return nullptr;
}
