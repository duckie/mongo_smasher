#pragma once
#include "randomizer.h"
#include "queue.h"
#include "utils.h"
#include <thread>
#include <bsoncxx/document/value.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/client.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/lock_types.hpp>
#include <memory>

namespace mongo_smasher {
namespace document_pool {

enum class update_method : size_t { latest, sample, update_method_MAX };


enum class thread_command_type : size_t { retrieve, stop };

struct ThreadCommand {
  thread_command_type type;
  std::string collection;
};

struct Document {
  size_t nb_used;
  std::shared_ptr<bsoncxx::document::value> value;
};

//
// DocumentPool class stores existing documents information for updates and finds
//
// To efficitently operate updates and finds, one must store information
// about existing documents. The goal here is to keep a register of existing documents
// updated regularly but not too much as not to impair real tests. Capacity and
// update method may be chosen by user
//
// For current implementation, each collection get its own thread. A better would 
// use only one for everybody. To study later on but much more complicated to 
// implement properly and efficiently.
//
class DocumentPool {
  // Shared randomizer
  Randomizer& randomizer_;

  // 
  update_method update_method_;

  // Size of the batch to draw documents from
  size_t const size_;

  // Number of times a document must be used before being discarded
  size_t const reuse_factor_;

  // Number of documents consumed : a single document may be accounted multiple times here
  std::atomic<size_t> nb_used_; 

  // This queue_ is used by the retrieval thread to know when it is supposed
  // to start a new retrieval or if it should just stop
  Queue<ThreadCommand> retrieve_queue_;

  // Mutex to protect the pool between the retrieval thread 
  // and the document pool
  boost::shared_mutex documents_mutex_;

  //
  std::vector<Document> documents_;

  //
  std::thread retrieval_thread_;

  //
  std::atomic<bool> retrieval_thread_working_;

 public:
  // Ctor launches the retrieval thread
  DocumentPool(Randomizer& randomizer, std::string const& db_uri, std::string const& db_name,
                std::string const& collection_name,
               update_method method, size_t size, size_t reuse_factor);

  ~DocumentPool();

  // Selects randomly a document and counts the retrieval, thus not const
  std::shared_ptr<bsoncxx::document::value> draw_document();
};

} // namespace document_pool

template <>
struct enum_view_size<document_pool::update_method> {
  static constexpr size_t const value = static_cast<size_t>(document_pool::update_method::update_method_MAX);
};

}  // namespace mongo_smasher
