#pragma once

#include "core/platform.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#if RE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

inline eastl::wstring string_to_wstring(const eastl::string& s)
{
#if RE_PLATFORM_WINDOWS
    DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);

    eastl::wstring result;
    result.resize(size);

    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);

    return result;
#else
    return eastl::wstring(); //todo
#endif
}

inline eastl::string wstring_to_string(const eastl::wstring& s)
{
#if RE_PLATFORM_WINDOWS
    DWORD size = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, NULL, 0, NULL, FALSE);

    eastl::string result;
    result.resize(size);

    WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, (LPSTR)result.c_str(), size, NULL, FALSE);

    return result;
#else
    return eastl::string(); //todo
#endif
}

inline void string_to_float_array(const eastl::string& str, eastl::vector<float>& output)
{
    const eastl::string delims = ",";

    auto first = eastl::cbegin(str);

    while (first != eastl::cend(str))
    {
        const auto second = eastl::find_first_of(first, eastl::cend(str),
            eastl::cbegin(delims), eastl::cend(delims));

        if (first != second)
        {
            output.push_back((float)atof(eastl::string(first, second).c_str()));
        }

        if (second == eastl::cend(str))
        {
            break;
        }

        first = eastl::next(second);
    }
}
