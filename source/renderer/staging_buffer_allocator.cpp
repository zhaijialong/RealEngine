#include "staging_buffer_allocator.h"
#include "renderer.h"
#include "utils/assert.h"

#define ALIGN(address, alignment) (((address) + (alignment) - 1) & ~((alignment) - 1)) 

StagingBufferAllocator::StagingBufferAllocator(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxBufferDesc desc;
    desc.size = 16 * 1024 * 1024;
    desc.memory_type = GfxMemoryType::CpuOnly;
    desc.alloc_type = GfxAllocationType::Placed;
    m_pBuffer.reset(pRenderer->GetDevice()->CreateBuffer(desc, "StagingBufferAllocator::m_pBuffer"));
}

StagingBuffer StagingBufferAllocator::Allocate(uint32_t size)
{
    RE_ASSERT(m_nAllocatedSize + size <= m_pBuffer->GetDesc().size);

    StagingBuffer buffer;
    buffer.buffer = m_pBuffer.get();
    buffer.size = size;
    buffer.offset = m_nAllocatedSize;

    m_nAllocatedSize += ALIGN(size, 256); //256 : D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT

    return buffer;
}

void StagingBufferAllocator::Reset()
{
    m_nAllocatedSize = 0;
}
