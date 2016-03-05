#pragma once
#include "randomizer.h"
#include "queue.h"
#include "utils.h"
#include <bsoncxx/document/value.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/client.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/lock_types.hpp>

namespace mongo_smasher {
namespace document_pool {

enum class update_method : size_t { latest, sample, update_method_MAX };


enum class retrieve_thread_command : size_t { retrieve, stop };

struct Document {
  size_t nb_used;
  bsoncxx::document::value value;
};



//
// DocumentPool class stores existing documents information for updates and finds
//
// To efficitently operate updates and finds, one must store information
// about existing documents. The goal here is to keep a register of existing documents
// updated regularly but not too much as not to impair real tests. Capacity and
// update method may be chosen by user
//
class DocumentPool {
  // Shared randomizer
  Randomizer& randomizer_;

  // DB Connection
  mongocxx::client db_conn_;

  // 
  update_method update_method_;

  // Size of the batch to draw documents from
  size_t size_;

  // Number of times a document must be used before being discarded
  size_t reuse_factor_;

  // This queue_ is used by the retrieval thread to now when it is supposed
  // to start a new retrieval of if it should just stop
  Queue<retrieve_thread_command> retrieve_queue_;

  // Mutex to protect the pool between the retrieval thread 
  // and the document pool
  boost::shared_mutex documents_mutex;

 public:
  DocumentPool(Randomizer& randomizer, std::string const& db_uri, std::string const& db_name,
               update_method method, size_t size, size_t reuse_factor);

  // Selects randomly a document and counts the retrieval, thus not const
  bsoncxx::document::view draw_document();
};

} // namespace document_pool

template <>
struct enum_view_size<document_pool::update_method> {
  static constexpr size_t const value = static_cast<size_t>(document_pool::update_method::update_method_MAX);
};

}  // namespace mongo_smasher
