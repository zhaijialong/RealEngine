#include "EASTL/allocator.h"
#include "stb/stb_sprintf.h"
#include "utils/memory.h"
#include <stdio.h> //vsnwprintf

#ifdef EASTL_USER_DEFINED_ALLOCATOR 
namespace eastl
{
    allocator gDefaultAllocator;
    allocator* GetDefaultAllocator()
    {
        return &gDefaultAllocator;
    }

    allocator::allocator(const char* EASTL_NAME(pName))
    {
#if EASTL_NAME_ENABLED
        mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
    }

    allocator::allocator(const allocator& EASTL_NAME(alloc))
    {
#if EASTL_NAME_ENABLED
        mpName = alloc.mpName;
#endif
    }

    allocator::allocator(const allocator&, const char* EASTL_NAME(pName))
    {
#if EASTL_NAME_ENABLED
        mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
    }

    allocator& allocator::operator=(const allocator& EASTL_NAME(alloc))
    {
#if EASTL_NAME_ENABLED
        mpName = alloc.mpName;
#endif
        return *this;
    }
    
    inline const char* allocator::get_name() const
    {
#if EASTL_NAME_ENABLED
        return mpName;
#else
        return EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
    }

    void allocator::set_name(const char* EASTL_NAME(pName))
    {
#if EASTL_NAME_ENABLED
        mpName = pName;
#endif
    }

    void* allocator::allocate(size_t n, int flags)
    {
        return RE_ALLOC(n);
    }

    void* allocator::allocate(size_t n, size_t alignment, size_t offset, int flags)
    {
        return RE_ALLOC(n, alignment);
    }

    void allocator::deallocate(void* p, size_t)
    {
        RE_FREE(p);
    }

    bool operator==(const allocator&, const allocator&)
    {
        return true; // All allocators are considered equal, as they merely use global new/delete.
    }

#if !defined(EA_COMPILER_HAS_THREE_WAY_COMPARISON)
    bool operator!=(const allocator&, const allocator&)
    {
        return false; // All allocators are considered equal, as they merely use global new/delete.
    }
#endif
} // namespace eastl
#endif // EASTL_USER_DEFINED_ALLOCATOR

int Vsnprintf8(char* p, size_t n, const char* pFormat, va_list arguments)
{
    return stbsp_vsnprintf(p, (int)n, pFormat, arguments);
}

int VsnprintfW(wchar_t* pDestination, size_t n, const wchar_t* pFormat, va_list arguments)
{
#ifdef _MSC_VER
    return _vsnwprintf(pDestination, n, pFormat, arguments);
#else
    return vsnwprintf(pDestination, n, pFormat, arguments);
#endif
}