#pragma once
#include "queue.h"
#include "randomizer.h"
#include "document_pool.h"
#include "collection_consumer.h"
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/value.hpp>
#include <mongocxx/cursor.hpp>
#include <regex>
#include <map>
#include <tuple>
#include <utility>

namespace mongo_smasher {

class FindUnit {
  document_pool::DocumentPool& pool_;
  typename ConsumerCommand::queue_t& queue_;
  bsoncxx::stdx::string_view name_;
  bsoncxx::document::view model_;
  double weight_;
  size_t bulk_size_;

  std::vector<bsoncxx::document::value> bulk_docs_;

  // Private member templates can be implemented in compilation unit
  //template <class T>
  //void process_value(T& ctx, ValueParams& value_params);
  template <class T>
  void process_element(T const& element, bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::array::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::document& ctx);

 public:
  FindUnit(document_pool::DocumentPool& pool, typename ConsumerCommand::queue_t& queue,
                 bsoncxx::stdx::string_view name, bsoncxx::document::view const& pattern,
                 double normalized_weight);

  double weight() const;
  typename ConsumerCommand::queue_t::duration_t process_tick();

  //// Returns total of documents produces since beginning
  //size_t nb_inserted() const;
};

}  // namespace mongo_smasher
