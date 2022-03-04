#include "eastl_allocator.h"
#include <stdio.h>

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

// Required by EASTL
int Vsnprintf8(char* p, size_t n, const char* pFormat, va_list arguments)
{
    return vsnprintf(p, n, pFormat, arguments);
}

int Vsnprintf16(char16_t* p, size_t n, const char16_t* pFormat, va_list arguments)
{
#ifdef _MSC_VER
    return _vsnwprintf((wchar_t*)p, n, (const wchar_t*)pFormat, arguments);
#else
    return vsnwprintf(p, n, pFormat, arguments);
#endif
}