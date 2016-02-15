#include "process_unit.h"
#include "logger.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

ProcessingUnit::ProcessingUnit(Randomizer &randomizer,
                               std::string const &db_uri,
                               bsx::stdx::string_view name,
                               bsoncxx::document::view const &collection)
    : randomizer_{randomizer}, db_conn_{mongocxx::uri{db_uri}}, name_{name},
      model_(collection["schema"]),
      bulk_size_(to_int<size_t>(collection,"bulk_size", 1u)) 
{
  bulk_docs_.reserve(bulk_size_);
  bulk_views_.reserve(bulk_size_);
}

void ProcessingUnit::process_element(bsoncxx::document::element const &element,
                                     bsx::builder::stream::array &ctx) {}

void ProcessingUnit::process_element(bsoncxx::document::element const &element,
                                     bsx::builder::stream::document &ctx) {
  if (element.type() == bsx::type::k_utf8) {
    auto value = to_str_view(element);
    if ('$' == value[0]) {

      ctx << element.key() << randomizer_.get_value_pusher(value.substr(1));
      //[&value,this](bsx::builder::stream::single_context s) {
      // randomizer_.get_value_pusher(value.substr(1))(s); };
    }
  } else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << element.key() << bsx::types::b_document{child.view()};
  }
}

void ProcessingUnit::process_tick() {
  auto db_collection = db_conn_["test"][name_];
  bsx::builder::stream::document document;
  for (auto value : model_.get_document().view()) {
    process_element(value, document);
  }
  bulk_docs_.emplace_back(std::move(document)); 
  bulk_views_.emplace_back(bulk_docs_.back().view());
  if (bulk_size_ <= bulk_docs_.size()) {
    db_collection.insert_many(bulk_views_);
    nb_instances_ += bulk_size_;
    bulk_docs_.clear();
    bulk_views_.clear();
  }
    
}

bsx::stdx::string_view ProcessingUnit::name() const { return name_; }

size_t ProcessingUnit::nb_inserted() const { return nb_instances_; }

} // namespace mongo_smasher
