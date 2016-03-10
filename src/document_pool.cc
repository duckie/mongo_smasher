#include "document_pool.h"
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
                           std::string const& db_name, std::string const& collection_name,
                           update_method method, size_t size, size_t reuse_factor)
    : randomizer_{randomizer},
      update_method_{method},
      retrieve_queue_{1},
      size_{size},
      reuse_factor_{reuse_factor},
      retrieval_thread_working_{false} {
  // Thread must copy data not kept by parent object
  retrieval_thread_ = thread([this, db_uri, db_name, collection_name]() {

    // Maintain a connection for this thread only
    client db_conn{uri{db_uri}};
    auto db_col = db_conn[db_name][collection_name];
    log(log_level::debug, "Start retrieval thread.\n");
    for (;;) {
      ThreadCommand command{this->retrieve_queue_.pop()};
      if (thread_command_type::stop == command.type) break;

      std::vector<Document> new_documents;
      new_documents.reserve(this->size_);
      // Get the documents
      if (update_method_ == update_method::latest) {
        using namespace bsoncxx::builder;
        options::find options{};
        options.sort(stream::document{} << "_id" << -1 << stream::finalize);
        options.limit(this->size_);

        auto cursor = db_col.find({}, options);
        for (auto&& doc_view : cursor) {
          new_documents.emplace_back(
              Document{0u, std::shared_ptr<document::value>(new document::value(doc_view))});
        }
      } else if (update_method_ == update_method::sample) {
        auto cursor = db_col.aggregate(pipeline{}.sample(this->size_));
        for (auto&& doc_view : cursor) {
          new_documents.emplace_back(
              Document{0u, std::shared_ptr<document::value>(new document::value(doc_view))});
        }
      }

      if (0 == new_documents.size()) {
        log(log_level::warning, "Document pool for %s.%s failed to retrieve any documents.",
            db_name.data(), collection_name.data());
      } else {
        documents_mutex_.lock();
        this->nb_used_ = 0u;
        std::swap(new_documents, this->documents_);
        documents_mutex_.unlock();
        retrieval_thread_working_ = false;
      }
    }
    log(log_level::debug, "Stop retrieval thread.\n");
  });

  // Push a first retrivel
  retrieve_queue_.push(ThreadCommand{thread_command_type::retrieve, {}});
}

DocumentPool::~DocumentPool() {
  retrieve_queue_.push(ThreadCommand{thread_command_type::stop, {}});
  retrieval_thread_.join();
}

std::shared_ptr<bsoncxx::document::value> DocumentPool::draw_document() {
  std::shared_ptr<bsoncxx::document::value> result {};

  if(documents_mutex_.try_lock_shared()) {
    if (documents_.size()) {
      size_t index = randomizer_.index_draw(documents_.size());
      auto& doc = documents_[index];
      result = doc.value;
      ++doc.nb_used;
      documents_mutex_.unlock_shared();
      if (!retrieval_thread_working_) {
        ++nb_used_;
        // Do not use <= or you would deadlock by spamming the retrieval
        // thread with multiple requests
        if (reuse_factor_ * size_ == nb_used_)
          retrieve_queue_.push(ThreadCommand{thread_command_type::retrieve, {}});
      }
    }
    else {
      documents_mutex_.unlock_shared();
      // Ask again for update
      if(0 == retrieve_queue_.size() && !retrieval_thread_working_)
          retrieve_queue_.push(ThreadCommand{thread_command_type::retrieve, {}});
    }
  }

  return result; 
}

}  // namespace document_pool

template <>
typename enum_view_definition<document_pool::update_method>::type
    enum_view_definition<document_pool::update_method>::str_array = {"latests", "sample"};
}  // namespace mongo_smasher
