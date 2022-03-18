#pragma once

#include "fmt/format.h"
#include <Windows.h>

inline void RE_LOG(const char* log)
{
    OutputDebugStringA(log);
}

template <typename... T>
inline void RE_LOG(const char* format, T&&... args)
{
    OutputDebugStringA(fmt::format(format, args...).c_str());
}
