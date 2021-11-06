#pragma once

#include "gfx/gfx.h"
#include <memory>

class RawBuffer
{
public:
    RawBuffer(const std::string& name);
    virtual ~RawBuffer() {}

    bool Create(uint32_t size, GfxMemoryType memory_type);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }

protected:
    std::string m_name;
    std::unique_ptr<IGfxBuffer> m_pBuffer;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
};
