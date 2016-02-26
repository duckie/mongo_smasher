#include "collection_producer.h"
#include "logger.h"

namespace mongo_smasher {

CollectionProducer::CollectionProducer(ThreadPilot& pilot,
    DocumentBatch::queue_t& queue, bsoncxx::document::view model,
                                       bsoncxx::stdx::string_view root_path)
    : pilot_{pilot}, queue_{queue}, randomizer_(model["values"].get_document().view(), root_path), model_(model) {
}

//CollectionProducer::CollectionProducer(CollectionProducer&& other) : queue_(other.queue_),
                                                          //randomizer_(std::move(other.randomizer_)),
                                                          //model_(std::move(other.model_)) {
//}

void CollectionProducer::run() {
  std::vector<ProcessingUnit> units;
  // ProcessingUnit unit{randomizer, db_uri,
  //auto view = model_["collections"].get_document().view();
  for (auto collection_view : model_["collections"].get_document().view()) {
    log(log_level::debug, "Registering %s\n", collection_view.key().data());
    units.emplace_back(randomizer_, queue_, collection_view.key(),
                       collection_view.get_document().view());
  }

  while (pilot_.run) {
    typename DocumentBatch::queue_t::duration_t idle_time {};
    for (auto& unit : units) {
      idle_time += unit.process_tick();
    }
  }
}

}  // namespace mongo_smasher
