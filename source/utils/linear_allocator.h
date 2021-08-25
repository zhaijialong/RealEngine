#pragma once

#include "memory.h"
#include "assert.h"

class LinearAllocator
{
public:
    LinearAllocator(uint32_t memory_size)
    {
        m_pMemory = RE_ALLOC(memory_size);
        m_nMemorySize = memory_size;
    }

    ~LinearAllocator()
    {
        RE_FREE(m_pMemory);
    }

    void* Alloc(uint32_t size, uint32_t alignment = 1)
    {
        uint32_t address = Align(m_nPointerOffset, alignment);
        RE_ASSERT(address + size <= m_nMemorySize);

        m_nPointerOffset = address + size;

        return (char*)m_pMemory + address;
    }

    void Reset()
    {
        m_nPointerOffset = 0;
    }

private:
    uint32_t Align(uint32_t address, uint32_t alignment)
    {
        return (address + alignment - 1) & ~(alignment - 1);
    }

private:
    void* m_pMemory = nullptr;
    uint32_t m_nMemorySize = 0;
    uint32_t m_nPointerOffset = 0;
};