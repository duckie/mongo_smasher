#include "collection_consumer.h"
#include "queue.h"
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>

namespace mongo_smasher {

  CollectionConsumer::CollectionConsumer(ThreadPilot& pilot, typename DocumentBatch::queue_t& queue, std::string db_uri)
    : pilot_{pilot}, queue_(queue), hub_{db_uri} 
  {}

  void CollectionConsumer::run() {
    while (pilot_.run) {
      typename DocumentBatch::queue_t::duration_t idle_time;
      auto batch = queue_.pop(&idle_time);
      idle_time_ += idle_time.count();
      hub_.get_collection(batch.db.to_string(), batch.col.to_string()).insert_many(batch.payload);
    }
  }
}  // namespace mongo_smasher
