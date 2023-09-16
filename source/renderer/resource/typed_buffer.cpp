#include "typed_buffer.h"
#include "core/engine.h"
#include "../renderer.h"

TypedBuffer::TypedBuffer(const eastl::string& name)
{
    m_name = name;
}

bool TypedBuffer::Create(GfxFormat format, uint32_t element_count, GfxMemoryType memory_type, bool uav)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    uint32_t stride = GetFormatRowPitch(format, 1);

    GfxBufferDesc desc;
    desc.stride = stride;
    desc.size = stride * element_count;
    desc.format = format;
    desc.memory_type = memory_type;
    desc.usage = GfxBufferUsageTypedBuffer;

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
    srvDesc.type = GfxShaderResourceViewType::TypedBuffer;
    srvDesc.format = format;
    srvDesc.buffer.size = stride * element_count;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (uav)
    {
        GfxUnorderedAccessViewDesc uavDesc;
        uavDesc.type = GfxUnorderedAccessViewType::TypedBuffer;
        uavDesc.format = format;
        uavDesc.buffer.size = stride * element_count;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pBuffer.get(), uavDesc, m_name));
        if (m_pUAV == nullptr)
        {
            return false;
        }
    }

    return true;
}
