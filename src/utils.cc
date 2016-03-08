#include "utils.h"
#include <sstream>
#include <iomanip>

using namespace bsoncxx;

namespace mongo_smasher {

std::chrono::high_resolution_clock::time_point parse_iso_date(std::string const& date) {
  return parse_iso_date(bsoncxx::stdx::string_view(date.data()));
}

std::chrono::high_resolution_clock::time_point parse_iso_date(bsoncxx::stdx::string_view date) {
  std::tm tm{};
  std::istringstream ss{date.data()};
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  return std::chrono::high_resolution_clock::from_time_t(std::mktime(&tm));
}

LooseElement::value::value() {
}

LooseElement::LooseElement() : type_{type::null} {
}

LooseElement::LooseElement(bsoncxx::document::view view) : type_{type::document} {
  value_.doc = std::move(view);
}

LooseElement::LooseElement(bsoncxx::array::view view) : type_{type::array} {
  value_.array = std::move(view);
}

LooseElement::LooseElement(bsoncxx::document::element element) : type_{type::doc_elem} {
  value_.doc_elem = std::move(element);
}

LooseElement::LooseElement(bsoncxx::array::element element) : type_{type::array_elem} {
  value_.array_elem = std::move(element);
}

LooseElement::LooseElement(LooseElement const& other) {
  type_ = other.type_;
  switch (type_) {
    case type::null:
      break;
    case type::document:
      value_.doc = other.value_.doc;
      break;
    case type::array:
      value_.array = other.value_.array;
      break;
    case type::doc_elem:
      value_.doc_elem = other.value_.doc_elem;
      break;
    case type::array_elem:
      value_.array_elem = other.value_.array_elem;
      break;
  }
}

LooseElement::LooseElement(LooseElement&& other) {
  type_ = other.type_;
  switch (type_) {
    case type::null:
      break;
    case type::document:
      value_.doc = std::move(other.value_.doc);
      break;
    case type::array:
      value_.array = std::move(other.value_.array);
      break;
    case type::doc_elem:
      value_.doc_elem = std::move(other.value_.doc_elem);
      break;
    case type::array_elem:
      value_.array_elem = std::move(other.value_.array_elem);
      break;
  }
}

LooseElement& LooseElement::operator=(LooseElement const& other) {
  clear();
  new (this) LooseElement(other);
  return *this;
}

LooseElement& LooseElement::operator=(LooseElement&& other) {
  clear();
  new (this) LooseElement(std::move(other));
  return *this;
}

LooseElement::~LooseElement() {
  clear();
}

void LooseElement::clear() {
  switch (type_) {
    case type::null:
      break;
    case type::document:
      value_.doc.bsoncxx::document::view::~view();
      break;
    case type::array:
      value_.array.bsoncxx::array::view::~view();
      break;
    case type::doc_elem:
      value_.doc_elem.bsoncxx::document::element::~element();
      break;
    case type::array_elem:
      value_.array_elem.bsoncxx::array::element::~element();
      break;
  }
}

LooseElement LooseElement::operator[](char const* key) const {
  return (*this)[bsoncxx::stdx::string_view(key)];
}

LooseElement LooseElement::operator[](bsoncxx::stdx::string_view key) const {
  switch (type_) {
    case type::document: {
      auto element = value_.doc.find(key);
      if (element != value_.doc.end()) {
        switch (element->type()) {
          case bsoncxx::type::k_document:
            return LooseElement(element->get_document().view());
          case bsoncxx::type::k_array:
            return LooseElement(element->get_array().value);
          default:
            return LooseElement(*element);
        }
      }
    } break;
    case type::doc_elem: {
      switch (value_.doc_elem.type()) {
        case bsoncxx::type::k_document: {
          auto doc = value_.doc_elem.get_document().view();
          auto element = doc.find(key);
          if (element != doc.end()) {
            switch (element->type()) {
              case bsoncxx::type::k_document:
                return LooseElement(element->get_document().view());
              case bsoncxx::type::k_array:
                return LooseElement(element->get_array().value);
              default:
                return LooseElement(*element);
            }
          }
        } break;
        default:
          break;
      }
    }
    default:
      break;
  }
  return {};
}

LooseElement LooseElement::operator[](std::string const& key) const {
  return (*this)[bsoncxx::stdx::string_view(key.data(), key.size())];
}

LooseElement LooseElement::operator[](size_t index) const {
  switch (type_) {
    case type::array: {
      auto element = value_.array.find(index);
      if (element != value_.array.end()) {
        switch (element->type()) {
          case bsoncxx::type::k_document:
            return LooseElement(element->get_document().view());
          case bsoncxx::type::k_array:
            return LooseElement(element->get_array().value);
          default:
            return LooseElement(*element);
        }
      }
    } break;
    case type::array_elem: {
      switch (value_.array_elem.type()) {
        case bsoncxx::type::k_array: {
          auto array = value_.array_elem.get_array().value;
          auto element = array.find(index);
          if (element != array.end()) {
            switch (element->type()) {
              case bsoncxx::type::k_document:
                return LooseElement(element->get_document().view());
              case bsoncxx::type::k_array:
                return LooseElement(element->get_array().value);
              default:
                return LooseElement(*element);
            }
          }
        } break;
        default:
          break;
      }
    }
    default:
      break;
  }
  return {};
}

}  // namespace mongo_smasher
