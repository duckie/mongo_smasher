#include "collection_consumer.h"
#include "queue.h"
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <cppformat/format.h>

namespace mongo_smasher {

CollectionHub::CollectionHub(std::string db_uri)
    : db_conn_{mongocxx::uri{db_uri}} {
}

mongocxx::collection& CollectionHub::get_collection(std::string const& db_name, std::string const& collection_name) {
  std::string full_name = fmt::format("{}/{}", collection_name, collection_name);
  auto col_it = collections_.find(full_name);
  if (end(collections_) == col_it) {
    col_it = collections_.emplace(full_name, db_conn_[db_name][collection_name]).first;
  }
  return col_it->second;
}

CollectionConsumer::CollectionConsumer(ThreadPilot& pilot, typename DocumentBatch::queue_t& queue,
                                       std::string db_uri)
    : pilot_{pilot}, queue_(queue), hub_{db_uri} {
}

void CollectionConsumer::run() {
  while (pilot_.run) {
    typename DocumentBatch::queue_t::duration_t idle_time;
    auto batch = queue_.pop(&idle_time);
    idle_time_ += idle_time.count();
    hub_.get_collection(batch.db.to_string(), batch.col.to_string()).insert_many(batch.payload);
  }
}

size_t CollectionConsumer::measure_idle_time() {
  size_t result = idle_time_;
  idle_time_ -= result;  // Not zero
  return result;
}
}  // namespace mongo_smasher
