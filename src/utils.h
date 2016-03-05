#pragma once
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <type_traits>
#include <chrono>
#include <cppformat/format.h>

namespace mongo_smasher {


// Compile time representation of the max value of an enum
template <class EnumType> struct enum_view_size;

// Static representation of a compile time enum <-> string association
template <class EnumType> struct enum_view_definition {
  using type = std::array<std::string, enum_view_size<EnumType>::value> const;
  static type str_array;
};

//
// Runtime representation of a compile time enum <-> string association
//
// enum_view implements a runtime behavior base on the compile time
// one.
//
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

// 
// Dummy inheritance used here to be specialized later
//
// TODO: Better specialized exceptions
//
struct exception : public std::exception {
};

// Extract the str_view from a document element
inline bsoncxx::stdx::string_view to_str_view(bsoncxx::document::element const& elem) {
  return elem.get_utf8().value;
}

// Extract the str_view from a array element
inline bsoncxx::stdx::string_view to_str_view(bsoncxx::array::element const& elem) {
  return elem.get_utf8().value;
}

//
// Interpret an element as a string
//
// To be used in compounds keys for instance.
//
template <class T> std::string to_string(T const& elem) {
  switch (elem.type()) {
    case bsoncxx::type::k_double:
      return fmt::format("{}", elem.get_double().value);
    case bsoncxx::type::k_utf8:
      return elem.get_utf8().value.to_string();
    case bsoncxx::type::k_document:
      return "{}";
    case bsoncxx::type::k_array:
      return "[]";
    case bsoncxx::type::k_undefined:
      return "undefined";
    case bsoncxx::type::k_oid:
      return elem.get_oid().value.to_string();
    case bsoncxx::type::k_bool:
      return elem.get_bool().value ? "true" : "false";
    case bsoncxx::type::k_date:
      // TODO: Manage properly formats here
      return fmt::format("{}", elem.get_date().value);
    case bsoncxx::type::k_null:
      return "null";
    case bsoncxx::type::k_regex:
      return elem.get_regex().regex.to_string();
    case bsoncxx::type::k_int32:
      return fmt::format("{}", elem.get_int32().value);
    case bsoncxx::type::k_int64:
      return fmt::format("{}", elem.get_int64().value);
    case bsoncxx::type::k_binary:
    case bsoncxx::type::k_dbpointer:
    case bsoncxx::type::k_code:
    case bsoncxx::type::k_symbol:
    case bsoncxx::type::k_codewscope:
    case bsoncxx::type::k_timestamp:
    case bsoncxx::type::k_maxkey:
    case bsoncxx::type::k_minkey:
    default:
      return {};
  };
}

// Extracts either int32 or int64 to the output type
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
