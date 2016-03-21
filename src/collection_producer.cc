#include "collection_producer.h"
#include "logger.h"
#include "utils.h"
#include "find_unit.h"
#include "insert_unit.h"

using namespace bsoncxx::stdx;
using namespace bsoncxx;

namespace mongo_smasher {

CollectionProducer::CollectionProducer(ThreadPilot& pilot, ConsumerCommand::queue_t& queue,
                                       bsoncxx::document::view model,
                                       bsoncxx::stdx::string_view root_path, std::string const& db_uri)
    : pilot_{pilot},
      queue_{queue},
      randomizer_(model["values"].get_document().view(), root_path),
      pool_{randomizer_, db_uri, document_pool::update_method::latest,10000,1000},
      model_(model),
      idle_time_{0} {
}

// CollectionProducer::CollectionProducer(CollectionProducer&& other) : queue_(other.queue_),
// randomizer_(std::move(other.randomizer_)),
// model_(std::move(other.model_)) {
//}

void CollectionProducer::run() {
  std::vector<InsertUnit> insert_units;
  std::vector<FindUnit> find_units;


  // Compute weight ratio to apply
  double max_weight{0.};
  auto process_weight=[&max_weight](double weight) {
    if (weight < 0.) weight = 0.;
    if (max_weight < weight) max_weight = weight;
  };
  for (auto collection_element : model_["collections"].get_document().view()) {
    auto collection_view = collection_element.get_document().view();
    auto find_patterns = collection_view["actions"].get_document().view()["find"].get_document().view()["patterns"].get_array().value;
    auto update_patterns = collection_view["actions"].get_document().view()["update"].get_document().view()["patterns"].get_array().value;
    auto delete_patterns = collection_view["actions"].get_document().view()["delete"].get_document().view()["patterns"].get_array().value;
    process_weight(LooseElement(collection_view)["actions"]["insert"]["weight"].get<double>(1.));
    for (auto find_element : find_patterns) {
      process_weight(find_element.get_document().view()["weight"].get_double().value);
    }
    for (auto update_element : update_patterns) {
      process_weight(update_element.get_document().view()["weight"].get_double().value);
    }
    for (auto delete_element : delete_patterns) {
      process_weight(delete_element.get_document().view()["weight"].get_double().value);
    }
  }

  for (auto collection_element : model_["collections"].get_document().view()) {
    log(log_level::debug, "Registering inserts for %s\n", collection_element.key().data());
    auto collection_view = collection_element.get_document().view();
    auto find_patterns = collection_view["actions"].get_document().view()["find"].get_document().view()["patterns"].get_array().value;
    auto update_patterns = collection_view["actions"].get_document().view()["update"].get_document().view()["patterns"].get_array().value;
    auto delete_patterns = collection_view["actions"].get_document().view()["delete"].get_document().view()["patterns"].get_array().value;
    insert_units.emplace_back(randomizer_, queue_, collection_element.key(),
                              collection_element.get_document().view(),
                              LooseElement(collection_view)["actions"]["insert"]["weight"].get<double>(1.) / max_weight);

    log(log_level::debug, "Registering finds for %s\n", collection_element.key().data());
    for (auto find_element : find_patterns) {
      process_weight(find_element.get_document().view()["weight"].get_double().value);
      find_units.emplace_back(pool_, queue_, collection_element.key(),
          find_element.get_document().view(),
          LooseElement(find_element)["weight"].get<double>(1.) / max_weight);
    }
    for (auto update_element : update_patterns) {
    }
    for (auto delete_element : delete_patterns) {
    }
  }


  while (pilot_.run) {
    typename ConsumerCommand::queue_t::duration_t idle_time{};
    for (auto& unit : insert_units) {
      idle_time += unit.process_tick();
    }
    for (auto& unit : find_units) {
      if (1. <= unit.weight() || randomizer_.existence_draw() <= unit.weight()) {
        idle_time += unit.process_tick();
      }
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
