#pragma once

#include "gfx/gfx.h"
#include "EASTL/unique_ptr.h"

class IndexBuffer
{
public:
    IndexBuffer(const eastl::string& name);

    bool Create(uint32_t stride, uint32_t index_count, GfxMemoryType memory_type);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    uint32_t GetIndexCount() const { return m_nIndexCount; }
    GfxFormat GetFormat() const { return m_pBuffer->GetDesc().format; }

private:
    eastl::string m_name;

    eastl::unique_ptr<IGfxBuffer> m_pBuffer;
    uint32_t m_nIndexCount = 0;
};