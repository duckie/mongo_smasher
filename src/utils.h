#pragma once
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <type_traits>

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

}
