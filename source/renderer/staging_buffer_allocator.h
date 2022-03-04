#pragma once

#include "../gfx/gfx.h"
#include "EASTL/unique_ptr.h"

class Renderer;

struct StagingBuffer
{
    IGfxBuffer* buffer;
    uint32_t size;
    uint32_t offset;
};

class StagingBufferAllocator
{
public:
    StagingBufferAllocator(Renderer* pRenderer);

    StagingBuffer Allocate(uint32_t size);
    void Reset();

private:
    void CreateNewBuffer();

private:
    Renderer* m_pRenderer = nullptr;
    eastl::vector<eastl::unique_ptr<IGfxBuffer>> m_pBuffers;
    uint32_t m_nCurrentBuffer = 0;
    uint32_t m_nAllocatedSize = 0;
    uint64_t m_nLastAllocatedFrame = 0;
};