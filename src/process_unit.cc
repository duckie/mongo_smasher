#include "process_unit.h"
#include "logger.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

ProcessingUnit::ProcessingUnit(Randomizer &randomizer,
                               std::string const &db_uri,
                               bsoncxx::document::view collections)
    : db_conn_{mongocxx::uri{db_uri}}, randomizer_{randomizer} {
  // First scan of the collections to determine the frequencies
  // TODO: use a SAT solver for dependencies
  for (auto collection_view : collections) {
    auto name = collection_view.key();
    float weight = 1.;
    // float weight = collection_view["weight"].get_double();
    collections_.emplace_back(
        Collection{name, collection_view["schema"], weight});
  }
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
      //randomizer_.get_value_pusher(value.substr(1))(s); };
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
  for (auto &collection : collections_) {
    bsx::builder::stream::document document;
    for (auto value : collection.model.get_document().view()) {
      process_element(value, document);
    }
    auto db_collection = db_conn_["test"][collection.name];
    db_collection.insert_one(document.view());
  }
}

}  // namespace mongo_smasher
