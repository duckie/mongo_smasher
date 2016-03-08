#include "collection_producer.h"
#include "logger.h"
#include "utils.h"

using namespace bsoncxx::stdx;
using namespace bsoncxx;

namespace mongo_smasher {

CollectionProducer::CollectionProducer(ThreadPilot& pilot, DocumentBatch::queue_t& queue,
                                       bsoncxx::document::view model,
                                       bsoncxx::stdx::string_view root_path)
    : pilot_{pilot},
      queue_{queue},
      randomizer_(model["values"].get_document().view(), root_path),
      model_(model),
      idle_time_ {0}
      {
}

// CollectionProducer::CollectionProducer(CollectionProducer&& other) : queue_(other.queue_),
// randomizer_(std::move(other.randomizer_)),
// model_(std::move(other.model_)) {
//}

void CollectionProducer::run() {
  std::vector<InsertUnit> insert_units;

  // Compute weight ratio to apply
  double max_weight{0.};
  std::map<string_view, double> weights;
  for (auto collection_element : model_["collections"].get_document().view()) {
    auto collection_view = collection_element.get_document().view();
    double weight = LooseElement(collection_view)["actions"]["insert"]["weight"].get<double>(1.);
    if (weight < 0.) weight = 0.;
    if (max_weight < weight) max_weight = weight;
    weights.emplace(collection_element.key(), weight);
  }

  for (auto collection_element : model_["collections"].get_document().view()) {
    log(log_level::debug, "Registering %s\n", collection_element.key().data());
    insert_units.emplace_back(randomizer_, queue_, collection_element.key(),
                       collection_element.get_document().view(),
                       weights[collection_element.key()] / max_weight);
  }

  while (pilot_.run) {
    typename DocumentBatch::queue_t::duration_t idle_time{};
    for (auto& unit : insert_units) {
      idle_time += unit.process_tick();
    }
    idle_time_ += idle_time.count();
  }
}

size_t CollectionProducer::measure_idle_time() {
  size_t result = idle_time_;
  idle_time_ -= result;  // Not zero
  return result;
}

}  // namespace mongo_smasher
