#include "process_unit.h"
#include "logger.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

ProcessingUnit::ProcessingUnit(Randomizer &randomizer,
                               std::string const &db_uri,
                               bsoncxx::document::element const &collection)
    : randomizer_{randomizer}, db_conn_{mongocxx::uri{db_uri}},
      name_{collection.key()},
      model_(collection.get_document().view()["schema"])
      {}

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
    db_collection.insert_one(document.view());
    ++nb_instances_;
}

bsx::stdx::string_view ProcessingUnit::name() const { 
  return name_;
}

size_t ProcessingUnit::nb_inserted() const { 
  return nb_instances_;
}

} // namespace mongo_smasher
