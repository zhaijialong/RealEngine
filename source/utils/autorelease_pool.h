#pragma once

#include "core/platform.h"
#if RE_PLATFORM_MAC || RE_PLATFORM_IOS

#include "Foundation/NSAutoreleasePool.hpp"

class ScopedAutoreleasePool
{
public:
    ScopedAutoreleasePool()
    {
        m_pPool = NS::AutoreleasePool::alloc()->init();
    }
    
    ~ScopedAutoreleasePool()
    {
        m_pPool->release();
    }
    
private:
    NS::AutoreleasePool* m_pPool = nullptr;
};

#endif // RE_PLATFORM_MAC || RE_PLATFORM_IOS
