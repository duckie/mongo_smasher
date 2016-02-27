#pragma once
#include "queue.h"
#include "randomizer.h"
#include "process_unit.h"
#include "collection_consumer.h"
#include <atomic>

namespace mongo_smasher {
class CollectionProducer {
  typename DocumentBatch::queue_t& queue_;
  ThreadPilot& pilot_;
  Randomizer randomizer_;
  bsoncxx::document::view model_;
  std::atomic<size_t> idle_time_;

 public:
  CollectionProducer(ThreadPilot& pilot,
      typename DocumentBatch::queue_t& queue,
                     bsoncxx::document::view model, bsoncxx::stdx::string_view root_path);
  CollectionProducer(CollectionProducer&&) = default;
  CollectionProducer(CollectionProducer const&) = delete;
  void run();

  // This not const nor it is a getter since 
  // it resets the idle time for the next measure
  size_t measure_idle_time();
};
}  // namespace mongo_smasher
