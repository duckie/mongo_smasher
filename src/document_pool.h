#pragma once

namespace mongo_smasher {
namespace document_pool {

enum class update_method : size_t { latest, sample, update_method_MAX };

template <>
struct enum_view_size<update_method> {
  static constexpr value const = static_cast<size_t>(update_method::update_method_MAX);
};

//
// DocumentPool class stores existing documents information
//
// To efficitently operate updates and finds, one must store information
// about existing documents. The goal here is to keep a register of existing documents
// updated regularly but not too much as not to impair real tests. Capacity and
// update method may be chosen by user
class DocumentPool {
 public:
  DocumentPool(Randomizer& randomizer, std::string const& db_uri, std::string const& db_name,
               update_method method, size_t max_size);
  bsoncxx::document::view draw_document();
};

}  // namespace document_pool
}  // namespace mongo_smasher
