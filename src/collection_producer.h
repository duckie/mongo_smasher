#pragma once
#include "queue.h"
#include "randomizer.h"
#include "process_unit.h"

namespace mongo_smasher {
class CollectionProducer {
  Queue<std::vector<bsoncxx::document::view>>& queue_;
  Randomizer randomizer;

 public:
  CollectionProducer(Queue<std::vector<bsoncxx::document::view>>& queue, bsoncxx::document::view model, bsoncxx::stdx::string_view root_path);
  void run();
};
}  // namespace mongo_smasher

