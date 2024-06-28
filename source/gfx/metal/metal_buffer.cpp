#include "metal_buffer.h"
#include "metal_device.h"

MetalBuffer::MetalBuffer(MetalDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalBuffer::~MetalBuffer()
{
    m_pBuffer->release();
}

bool MetalBuffer::Create()
{

    return true;
}

uint64_t MetalBuffer::GetGpuAddress()
{
    return m_pBuffer->gpuAddress();
}

uint32_t MetalBuffer::GetRequiredStagingBufferSize() const
{
    return (uint32_t)m_pBuffer->allocatedSize();
}
