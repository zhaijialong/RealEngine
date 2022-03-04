#include "index_buffer.h"
#include "../renderer.h"
#include "core/engine.h"
#include "utils/assert.h"

IndexBuffer::IndexBuffer(const eastl::string& name)
{
    m_name = name;
}

bool IndexBuffer::Create(uint32_t stride, uint32_t index_count, GfxMemoryType memory_type)
{
    RE_ASSERT(stride == 2 || stride == 4);
    m_nIndexCount = index_count;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxBufferDesc desc;
    desc.stride = stride;
    desc.size = stride * index_count;
    desc.format = stride == 2 ? GfxFormat::R16UI : GfxFormat::R32UI;
    desc.memory_type = memory_type;

    m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
    if (m_pBuffer == nullptr)
    {
        return false;
    }

    return true;
}
