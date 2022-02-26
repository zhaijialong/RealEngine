#pragma once

#include "fmt/format.h"
#include <Windows.h>

#define RE_LOG(...) OutputDebugStringA(fmt::format(__VA_ARGS__).c_str())