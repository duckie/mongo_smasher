#pragma once
#include <vector>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/collection.hpp>
#include "mongo_smasher.h"
#include "queue.h"

namespace mongo_smasher {

class CollectionHub {
  mongocxx::client db_conn_;
  std::map<std::string, mongocxx::collection> collections_;

 public:
  CollectionHub(std::string db_uri);
  mongocxx::collection& get_collection(std::string const& db_name, std::string const& collection_name);
};


struct DocumentBatch {
  using payload_t = std::vector<bsoncxx::document::value>;
  using queue_t = Queue<DocumentBatch>;
  bsoncxx::stdx::string_view db;
  bsoncxx::stdx::string_view col;
  payload_t payload;
};

class CollectionConsumer {
  ThreadPilot& pilot_;
  typename DocumentBatch::queue_t& queue_;
  CollectionHub hub_;
  std::atomic<size_t> idle_time_;

 public:
  CollectionConsumer(ThreadPilot& pilot, typename DocumentBatch::queue_t& queue,
                     std::string db_uri);
  void run();

  // This not const nor it is a getter since
  // it resets the idle time for the next measure
  size_t measure_idle_time();
};

}  // namespace mongo_smasher
