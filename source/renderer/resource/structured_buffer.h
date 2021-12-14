#pragma once

#include "gfx/gfx.h"
#include <memory>

class StructuredBuffer
{
public:
    StructuredBuffer(const std::string& name);

    bool Create(uint32_t stride, uint32_t element_count, GfxMemoryType memory_type, bool uav);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV() const { return m_pUAV.get(); }

protected:
    std::string m_name;
    std::unique_ptr<IGfxBuffer> m_pBuffer;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
    std::unique_ptr<IGfxDescriptor> m_pUAV;
};