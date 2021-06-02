#pragma once

#include "gfx/gfx.h"
#include <memory>

class StructuredBuffer
{
public:
    StructuredBuffer(const std::string& name);
    virtual ~StructuredBuffer() {}

    bool Create(uint32_t stride, uint32_t element_count, GfxMemoryType memory_type);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }

protected:
    std::string m_name;
    std::unique_ptr<IGfxBuffer> m_pBuffer;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
};

class RWStructuredBuffer : public StructuredBuffer
{
private:
    std::unique_ptr<IGfxDescriptor> m_pUAV;
};