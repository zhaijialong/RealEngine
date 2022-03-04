#include "staging_buffer_allocator.h"
#include "renderer.h"
#include "utils/assert.h"

#define BUFFER_SIZE (64 * 1024 * 1024)
#define ALIGN(address, alignment) (((address) + (alignment) - 1) & ~((alignment) - 1)) 

StagingBufferAllocator::StagingBufferAllocator(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

StagingBuffer StagingBufferAllocator::Allocate(uint32_t size)
{
    RE_ASSERT(size <= BUFFER_SIZE);

    if (m_pBuffers.empty())
    {
        CreateNewBuffer();
    }

    if (m_nAllocatedSize + size > BUFFER_SIZE)
    {
        CreateNewBuffer();
        m_nCurrentBuffer++;
        m_nAllocatedSize = 0;
    }

    StagingBuffer buffer;
    buffer.buffer = m_pBuffers[m_nCurrentBuffer].get();
    buffer.size = size;
    buffer.offset = m_nAllocatedSize;

    m_nAllocatedSize += ALIGN(size, 512); //512 : D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
    m_nLastAllocatedFrame = m_pRenderer->GetFrameID();

    return buffer;
}

void StagingBufferAllocator::Reset()
{
    m_nCurrentBuffer = 0;
    m_nAllocatedSize = 0;

    if (!m_pBuffers.empty())
    {
        if (m_pRenderer->GetFrameID() - m_nLastAllocatedFrame > 100)
        {
            m_pBuffers.clear();
        }
    }
}

void StagingBufferAllocator::CreateNewBuffer()
{
    GfxBufferDesc desc;
    desc.size = BUFFER_SIZE;
    desc.memory_type = GfxMemoryType::CpuOnly;

    IGfxBuffer* buffer = m_pRenderer->GetDevice()->CreateBuffer(desc, "StagingBufferAllocator::m_pBuffer");
    m_pBuffers.push_back(eastl::unique_ptr<IGfxBuffer>(buffer));
}
