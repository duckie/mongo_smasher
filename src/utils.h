#pragma once
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <type_traits>
#include <chrono>

namespace mongo_smasher {
// Snippet used to simplify the enum <-> string relationship without macros

template <class EnumType> struct enum_view_size;

template <class EnumType> struct enum_view_definition {
  using type = std::array<std::string, enum_view_size<EnumType>::value> const;
  static type str_array;
};

template <class EnumType> struct enum_view {
  static_assert(
      std::is_enum<EnumType>::value &&
          std::is_convertible<typename std::underlying_type<EnumType>::type,
                              size_t>::value,
      "The enum underlying type must be convertible to size_t");

  static EnumType from_string(std::string const &value) {
    using namespace std;
    auto const &str_array = enum_view_definition<EnumType>::str_array;
    return static_cast<EnumType>(
        distance(begin(str_array), find(begin(str_array), end(str_array), value)));
  }

  static std::string to_string(EnumType value) {
    return enum_view_definition<EnumType>::str_array[static_cast<size_t>(
        value)];
  }
};

struct exception : public std::exception {
};

inline bsoncxx::stdx::string_view to_str_view(bsoncxx::document::element const& elem) {
  return elem.get_utf8().value;
}

inline bsoncxx::stdx::string_view to_str_view(bsoncxx::array::element const& elem) {
  return elem.get_utf8().value;
}

template <class T>  
T to_int(bsoncxx::document::view const& view, bsoncxx::stdx::string_view name, T default_value) {
  auto view_it = view.find(name);
  if (view_it != view.end()) {
    auto elem = *view_it;
    if (elem.type() == bsoncxx::type::k_int64)
      return static_cast<T>(elem.get_int64().value);
    else if (elem.type() == bsoncxx::type::k_int32)
      return static_cast<T>(elem.get_int32().value);
  }
  return default_value;
}

std::chrono::high_resolution_clock::time_point parse_iso_date(std::string const& date);
std::chrono::high_resolution_clock::time_point parse_iso_date(bsoncxx::stdx::string_view date);

}
