#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <algorithm>

inline std::wstring string_to_wstring(const std::string& s)
{
    DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);

    std::wstring result;
    result.resize(size);

    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);

    return result;
}

inline std::string wstring_to_string(const std::wstring& s)
{
    DWORD size = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, NULL, 0, NULL, FALSE);

    std::string result;
    result.resize(size);

    WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, (LPSTR)result.c_str(), size, NULL, FALSE);

    return result;
}

inline void string_to_float_array(const std::string& str, std::vector<float>& output)
{
    const std::string delims = ",";

    auto first = std::cbegin(str);

    while (first != std::cend(str))
    {
        const auto second = std::find_first_of(first, std::cend(str),
            std::cbegin(delims), std::cend(delims));

        if (first != second)
        {
            output.push_back(std::stof(std::string(first, second)));
        }

        if (second == std::cend(str))
        {
            break;
        }

        first = std::next(second);
    }
}