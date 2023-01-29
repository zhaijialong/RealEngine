#pragma once

#include "fmt/format.h"
#include "EASTL/string.h"

template<>
struct fmt::formatter<eastl::string> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const eastl::string& val, FormatContext& ctx) const
    {
        return formatter<string_view>::format(val.c_str(), ctx);
    }
};