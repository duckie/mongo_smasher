#pragma once
#include "queue.h"
#include "randomizer.h"
#include "insert_unit.h"
#include "collection_consumer.h"
#include <atomic>

namespace mongo_smasher {

//
// CollectionProducer generates documents based on schemas onto the queue
//
// CollectionProducer is responsible for allocating memory and using CPU
// ressources to produce randomized documents.
//
class CollectionProducer {
  typename DocumentBatch::queue_t& queue_;
  ThreadPilot& pilot_;
  Randomizer randomizer_;
  bsoncxx::document::view model_;
  std::atomic<size_t> idle_time_;

 public:
  //
  // Ctor
  //
  // @param pilot is keeping CollectionProducer::run() alive while true
  // @param queue is meant to be fed produced documents
  // @param model contains the whole collection object
  //              which is $root_object.collections.$collection_name
  // @param root_path to be set at the root path of the .json root file
  //                  needed to interpret coorectly relative pathes
  //
  CollectionProducer(ThreadPilot& pilot, typename DocumentBatch::queue_t& queue,
                     bsoncxx::document::view model, bsoncxx::stdx::string_view root_path);
  CollectionProducer(CollectionProducer&&) = default;
  CollectionProducer(CollectionProducer const&) = delete;
  
  // 
  // Loops until advised to stop by the thread pilot
  //
  // Each loop calls a tick on processing units
  //
  void run();

  // This is not const nor it is a getter since
  // it resets the idle time for the next measure
  size_t measure_idle_time();
};
}  // namespace mongo_smasher
