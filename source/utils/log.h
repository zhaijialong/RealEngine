#pragma once

#include "fmt.h"
#include "spdlog/spdlog.h"

#define RE_TRACE(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::trace, __VA_ARGS__)
#define RE_DEBUG(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::debug, __VA_ARGS__)
#define RE_INFO(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::info, __VA_ARGS__)
#define RE_WARN(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::warn, __VA_ARGS__)
#define RE_ERROR(...) spdlog::default_logger_raw()->log(spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, spdlog::level::err, __VA_ARGS__)

