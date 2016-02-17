#include "utils.h"
#include <sstream>
#include <iomanip>

namespace mongo_smasher {
std::chrono::high_resolution_clock::time_point parse_iso_date(std::string const& date) {
  return parse_iso_date(bsoncxx::stdx::string_view(date.data()));
}
std::chrono::high_resolution_clock::time_point parse_iso_date(bsoncxx::stdx::string_view date) {
  std::tm tm {};
  std::istringstream ss {date.data()};
  ss >> std::get_time(&tm,"%Y-%m-%d %H:%M:%S");
  return std::chrono::high_resolution_clock::from_time_t(std::mktime(&tm));
}
}  // namespace mongo_smasher
