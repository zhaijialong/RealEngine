#pragma once

#include "../gfx/gfx.h"
#include <memory>

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
    Renderer* m_pRenderer = nullptr;
    std::unique_ptr<IGfxBuffer> m_pBuffer;
    uint32_t m_nAllocatedSize = 0;
};