#include "collection_producer.h"
#include "logger.h"

namespace mongo_smasher {

CollectionProducer::CollectionProducer(DocumentBatch::queue_t& queue,
                                       bsoncxx::document::view model,
                                       bsoncxx::stdx::string_view root_path)
    : queue_{queue}, randomizer_(model["values"].get_document().view(), root_path), model_(model) {
}

void CollectionProducer::run() {
  std::vector<ProcessingUnit> units;
  // ProcessingUnit unit{randomizer, db_uri,
  auto view = model_["collections"].get_document().view();
  for (auto collection_view : view["collections"].get_document().view()) {
    log(log_level::debug, "Registering %s\n", collection_view.key().data());
    units.emplace_back(randomizer_, queue_, collection_view.key(),
                       collection_view.get_document().view());
  }
}

}  // namespace mongo_smasher
