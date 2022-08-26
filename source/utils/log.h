#pragma once

#include "fmt/format.h"
#include <Windows.h>

inline void RE_LOG(const char* log)
{
    OutputDebugStringA(log);
    OutputDebugStringA("\n");
}

template <typename... T>
inline void RE_LOG(const char* format, T&&... args)
{
    RE_LOG(fmt::format(format, args...).c_str());
}
