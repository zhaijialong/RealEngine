#pragma once

#include "gfx/gfx.h"
#include "EASTL/unique_ptr.h"

class TypedBuffer
{
public:
    TypedBuffer(const eastl::string& name);

    bool Create(GfxFormat format, uint32_t element_count, GfxMemoryType memory_type, bool uav);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV() const { return m_pUAV.get(); }

protected:
    eastl::string m_name;
    eastl::unique_ptr<IGfxBuffer> m_pBuffer;
    eastl::unique_ptr<IGfxDescriptor> m_pSRV;
    eastl::unique_ptr<IGfxDescriptor> m_pUAV;
};