#pragma once
#include <vector>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/collection.hpp>
#include "mongo_smasher.h"
#include "queue.h"

namespace mongo_smasher {

//
// CollectionHub maintains alive a connection and collections proxies
//
// Collection proxies life cycle is related to the parent connection,
// the CollectionHub just held them together to simplify.
//
class CollectionHub {
  mongocxx::client db_conn_;
  std::map<std::string, mongocxx::collection> collections_;

 public:
  CollectionHub(std::string db_uri);
  mongocxx::collection& get_collection(std::string const& db_name, std::string const& collection_name);
};

enum class command_type : unsigned int {
  insert_one,
  insert_many,
  update_one,
  update_many,
  delete_one,
  delete_many,
  find,
  command_type_MAX
};

// 
// The ConsumerCommand stores destination and documents
//
// Instances of this class are supposed to be exchanged between
// producers and consumers through a queue.
//
struct ConsumerCommand {
  using payload_t = std::vector<bsoncxx::document::value>;
  using queue_t = Queue<ConsumerCommand>;
  command_type type;
  bsoncxx::stdx::string_view db;
  bsoncxx::stdx::string_view col;
  payload_t payload;
  mongocxx::options::insert insert_options;
  mongocxx::options::update update_options;
  mongocxx::options::find find_options;
  mongocxx::options::delete_options del_options;
};

//
// CollectionConsumer stores document into DBs
//
// A CollectionConsumer, when ran in a thread, reads from the queue
// and publishes received documents. Each CollectionConsumer holds
// its own CollectionHub.
//
class CollectionConsumer {
  ThreadPilot& pilot_;
  typename ConsumerCommand::queue_t& queue_;
  CollectionHub hub_;
  std::atomic<size_t> idle_time_;

 public:
  CollectionConsumer(ThreadPilot& pilot, typename ConsumerCommand::queue_t& queue,
                     std::string db_uri);

  // Read from the queue until advised to stop by the ThreadPilot
  void run();

  // This not const nor it is a getter since
  // it resets the idle time for the next measure
  size_t measure_idle_time();
};

}  // namespace mongo_smasher
