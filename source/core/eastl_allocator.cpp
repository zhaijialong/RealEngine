#include "eastl_allocator.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"
#include "rpmalloc/rpmalloc.h"
#include <stdio.h> //vsnwprintf

// Required by EASTL

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return rpmalloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return rpaligned_alloc(alignment, size);
}

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