#pragma once

#include "spdlog/spdlog.h"

#define RE_LOG(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::trace, __VA_ARGS__)

