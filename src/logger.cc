#include "logger.h"

namespace mongo_smasher {

template <>
typename enum_view_definition<log_level>::type enum_view_definition<log_level>::str_array = {
    "debug", "info", "warning", "error", "fatal", "quiet"};

log_level &mutable_global_log_level() {
  static log_level global_level{log_level::info};
  return global_level;
}

log_level global_log_level() {
  return mutable_global_log_level();
}

}  // namespace mongo_smasher
