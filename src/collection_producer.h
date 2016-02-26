#pragma once
#include "queue.h"
#include "randomizer.h"
#include "process_unit.h"
#include "collection_consumer.h"

namespace mongo_smasher {
class CollectionProducer {
  typename DocumentBatch::queut_t& queue_;
  Randomizer randomizer_;
  bsoncxx::document::view model_;

 public:
  CollectionProducer(Queue<std::vector<bsoncxx::document::view>>& queue,
                     bsoncxx::document::view model, bsoncxx::stdx::string_view root_path);
  void run();
};
}  // namespace mongo_smasher
