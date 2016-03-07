#pragma once
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <type_traits>
#include <chrono>
#include <cppformat/format.h>
#include <type_traits>

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


template <class T, class Element>  
T to_number(Element element,T default_value) {
    switch (element.type()) {
    case bsoncxx::type::k_int64:
      return static_cast<T>(element.get_int64().value);
    case bsoncxx::type::k_int32:
      return static_cast<T>(element.get_int32().value);
    case bsoncxx::type::k_double:
      return static_cast<T>(element.get_double().value);
    default:
      break;
    }
    return default_value;
}

template <class T>  
T to_int(bsoncxx::document::view const& view, bsoncxx::stdx::string_view name, T default_value) {
  auto element = view.find(name);
  if (element != view.end()) {
    return to_number<T>(*element,default_value);
  }
  return default_value;
}

class LooseDocumentView {
  enum class type { null, doc_elem, array_elem, document, array };
  union value {
    value();
    ~value() = default;
    bsoncxx::document::view doc;
    bsoncxx::array::view array;
    bsoncxx::document::element doc_elem;
    bsoncxx::array::element array_elem;
  };

  type type_;
  value value_;

  void clear();

 public:
  LooseDocumentView();
  LooseDocumentView(bsoncxx::document::view view);
  LooseDocumentView(bsoncxx::array::view view);
  LooseDocumentView(bsoncxx::document::element element);
  LooseDocumentView(bsoncxx::array::element element);
  LooseDocumentView(LooseDocumentView const&);
  LooseDocumentView(LooseDocumentView &&);
  LooseDocumentView& operator=(LooseDocumentView const&);
  LooseDocumentView& operator=(LooseDocumentView &&);
  ~LooseDocumentView();

  LooseDocumentView operator[] (char const* key);
  LooseDocumentView operator[] (bsoncxx::stdx::string_view key);
  LooseDocumentView operator[] (std::string const& key);
  template <size_t Size> inline LooseDocumentView operator[] (char const (&key)[Size]) {
    return (*this)[bsoncxx::stdx::string_view{key,Size}];
  }

  LooseDocumentView operator[] (size_t index);

  template <class T> typename std::enable_if<std::is_same<std::string,T>::value,T>::type get(std::string def = {}) {
    switch (type_) {
      case type::doc_elem:
        return to_string(value_.doc_elem);
      case type::array_elem:
        return to_string(value_.array_elem);
        break;
      default:
        break;
    }
    return def;
  }

  template <class T> typename std::enable_if<std::is_same<bsoncxx::stdx::string_view,T>::value,T>::type get(bsoncxx::stdx::string_view def = {}) {
    switch (type_) {
      case type::doc_elem:
        switch (value_.doc_elem.type()) {
          case bsoncxx::type::k_utf8:
            return value_.doc_elem.get_utf8().value;
          default:
            break;
        }
        break;
      case type::array_elem:
        switch (value_.array_elem.type()) {
          case bsoncxx::type::k_utf8:
            return value_.array_elem.get_utf8().value;
          default:
            break;
        }
        break;
      default:
        break;
    }
    return def;
  }

  template <class T> typename std::enable_if<std::is_arithmetic<T>::value,T>::type get(T def = {}) {
    switch (type_) {
      case type::doc_elem:
        return to_number<T>(value_.doc_elem,def);
      case type::array_elem:
        return to_number<T>(value_.array_elem,def);
        break;
      default:
        break;
    }
    return def;
  }
};

std::chrono::high_resolution_clock::time_point parse_iso_date(std::string const& date);
std::chrono::high_resolution_clock::time_point parse_iso_date(bsoncxx::stdx::string_view date);

}
