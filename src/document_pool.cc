#include "document_pool.h"
#include <tuple>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/pipeline.hpp>
#include "logger.h"

using namespace std;
using namespace bsoncxx;
using namespace mongocxx;

namespace mongo_smasher {
namespace document_pool {

DocumentPool::DocumentPool(Randomizer& randomizer, std::string const& db_uri,
                           update_method method, size_t size, size_t reuse_factor)
    : randomizer_{randomizer},
      update_method_{method},
      retrieve_queue_{1},
      size_{size},
      reuse_factor_{reuse_factor},
      retrieval_thread_working_{false} {
  // Thread must copy data not kept by parent object
  retrieval_thread_ = thread([this, db_uri]() {

    // Maintain a connection for this thread only
    client db_conn{uri{db_uri}};
    std::map<std::string,mongocxx::collection> dbs;
    log(log_level::debug, "Start retrieval thread.\n");
    for (;;) {
      ThreadCommand command{this->retrieve_queue_.pop()};
      if (thread_command_type::stop == command.type) break;

      std::string key = fmt::format("{}/{}", command.db, command.collection);

      // Get database link
      auto db_it = dbs.find(key);
      if (end(dbs) == db_it) {
        tie(db_it,ignore) = dbs.insert(make_pair(key, db_conn[command.db][command.collection]));
      }
      auto& db = db_it->second;


      // Get documents
      Documents::document_list_t new_documents;
      new_documents.reserve(this->size_);
      if (update_method_ == update_method::latest) {
        using namespace bsoncxx::builder;
        options::find options{};
        options.sort(stream::document{} << "_id" << -1 << stream::finalize);
        options.limit(this->size_);

        auto cursor = db.find({}, options);
        for (auto&& doc_view : cursor) {
          new_documents.emplace_back(new document::value(doc_view));
        }
      } else if (update_method_ == update_method::sample) {
        auto cursor = db.aggregate(pipeline{}.sample(this->size_));
        for (auto&& doc_view : cursor) {
          new_documents.emplace_back(new document::value(doc_view));
        }
      }

      if (0 == new_documents.size()) {
        log(log_level::warning, "Document pool for %s.%s failed to retrieve any documents.",
            command.db.data(), command.collection.data());
      } else {
        lock_guard<mutex> lock(documents_mutex_);
        auto& current_documents = documents_[key];
        current_documents.nb_used = 0;
        std::swap(new_documents, current_documents.values);
        retrieval_thread_working_ = false;
      }
    }
    log(log_level::debug, "Stop retrieval thread.\n");
  });
}

DocumentPool::~DocumentPool() {
  retrieve_queue_.push(ThreadCommand{thread_command_type::stop, {}});
  retrieval_thread_.join();
}

std::shared_ptr<bsoncxx::document::value> DocumentPool::draw_document(std::string const& db_name, std::string const& col_name) {
  std::shared_ptr<bsoncxx::document::value> result {};
  auto key = fmt::format("{}/{}", db_name, col_name);

  if(documents_mutex_.try_lock()) {
    auto& current_documents = documents_[key];
    if (current_documents.values.size()) {
      size_t index = randomizer_.index_draw(current_documents.values.size());
      auto& doc = current_documents.values[index];
      result = doc;
      ++current_documents.nb_used;
      documents_mutex_.unlock();
      if (!retrieval_thread_working_) {
        // Do not use <= or you would deadlock by spamming the retrieval
        // thread with multiple requests
        if (reuse_factor_ * size_ == current_documents.nb_used)
          retrieve_queue_.push(ThreadCommand{thread_command_type::retrieve, db_name, col_name});
      }
    }
    else {
      documents_mutex_.unlock();
      // Ask again for update
      if(0 == retrieve_queue_.size() && !retrieval_thread_working_)
          retrieve_queue_.push(ThreadCommand{thread_command_type::retrieve, db_name, col_name});
    }
  }

  return result; 
}

}  // namespace document_pool

template <>
typename enum_view_definition<document_pool::update_method>::type
    enum_view_definition<document_pool::update_method>::str_array = {"latests", "sample"};
}  // namespace mongo_smasher
