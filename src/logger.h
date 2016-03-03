#pragma once
#include <cstddef>
#include "utils.h"
#include <cppformat/format.h>

namespace mongo_smasher {
enum class log_level : size_t {
  debug,
  info,
  warning,
  error,
  fatal,
  quiet,
  LOG_LEVEL_MAX
};

template <> struct enum_view_size<log_level> {
  static constexpr size_t const value = static_cast<size_t>(log_level::LOG_LEVEL_MAX);
};

log_level& mutable_global_log_level();
log_level global_log_level();

template <typename... T>
void raw_log(log_level level, char const *format, T &&... args) {
  fmt::printf("[%s] ", enum_view<log_level>::to_string(level).c_str());
  fmt::printf(format, args...);
}

template <typename... T>
void log(log_level level, char const *format, T &&... args) {
  if (static_cast<size_t>(global_log_level()) <= static_cast<size_t>(level))
    raw_log(level,format,args...);
}

}  // namespace mongo_smasher
