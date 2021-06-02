#pragma once

#include "gfx/gfx.h"
#include <memory>

class IndexBuffer
{
public:
    IndexBuffer(const std::string& name);

    bool Create(uint32_t stride, uint32_t index_count, GfxMemoryType memory_type);

    IGfxBuffer* GetBuffer() const { return m_pBuffer.get(); }
    uint32_t GetIndexCount() const { return m_nIndexCount; }

private:
    std::string m_name;

    std::unique_ptr<IGfxBuffer> m_pBuffer;
    uint32_t m_nIndexCount = 0;
};