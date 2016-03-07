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

LooseDocumentView::value::value() {
}

LooseDocumentView::LooseDocumentView() : type_{type::null} {
}

LooseDocumentView::LooseDocumentView(bsoncxx::document::view view) : type_{type::document} {
  value_.doc = std::move(view);
}

LooseDocumentView::LooseDocumentView(bsoncxx::array::view view) : type_{type::array} {
  value_.array = std::move(view);
}

LooseDocumentView::LooseDocumentView(bsoncxx::document::element element) : type_{type::doc_elem} {
  value_.doc_elem = std::move(element);
}

LooseDocumentView::LooseDocumentView(bsoncxx::array::element element) : type_{type::array_elem} {
  value_.array_elem = std::move(element);
}

LooseDocumentView::LooseDocumentView(LooseDocumentView const& other) {
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

LooseDocumentView::LooseDocumentView(LooseDocumentView && other) {
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

LooseDocumentView& LooseDocumentView::operator=(LooseDocumentView const& other) {
  clear();
  new (this) LooseDocumentView(other);
  return *this;
}

LooseDocumentView& LooseDocumentView::operator=(LooseDocumentView && other) {
  clear();
  new (this) LooseDocumentView(std::move(other));
  return *this;
}

LooseDocumentView::~LooseDocumentView() {
  clear();
}

void LooseDocumentView::clear() {
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

LooseDocumentView LooseDocumentView::operator[](char const* key) {
  return (*this)[bsoncxx::stdx::string_view(key)];
}

LooseDocumentView LooseDocumentView::operator[](bsoncxx::stdx::string_view key) {
  switch (type_) {
    case type::document: {
      auto element = value_.doc.find(key);
      if (element != value_.doc.end()) {
        switch (element->type()) {
          case bsoncxx::type::k_document:
            return LooseDocumentView(element->get_document().view());
          case bsoncxx::type::k_array:
            return LooseDocumentView(element->get_array().value);
          default:
            return LooseDocumentView(*element);
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
                return LooseDocumentView(element->get_document().view());
              case bsoncxx::type::k_array:
                return LooseDocumentView(element->get_array().value);
              default:
                return LooseDocumentView(*element);
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

LooseDocumentView LooseDocumentView::operator[](std::string const& key) {
  return (*this)[bsoncxx::stdx::string_view(key.data(), key.size())];
}

LooseDocumentView LooseDocumentView::operator[](size_t index) {
  switch (type_) {
    case type::array: {
      auto element = value_.array.find(index);
      if (element != value_.array.end()) {
        switch (element->type()) {
          case bsoncxx::type::k_document:
            return LooseDocumentView(element->get_document().view());
          case bsoncxx::type::k_array:
            return LooseDocumentView(element->get_array().value);
          default:
            return LooseDocumentView(*element);
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
                return LooseDocumentView(element->get_document().view());
              case bsoncxx::type::k_array:
                return LooseDocumentView(element->get_array().value);
              default:
                return LooseDocumentView(*element);
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
