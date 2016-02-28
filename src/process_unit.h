#pragma once
#include "queue.h"
#include "randomizer.h"
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

class CollectionHub;

struct Collection {
  bsoncxx::stdx::string_view name;
  bsoncxx::document::element model;
  double weight;
};

enum class key_category { simple, array };

struct KeyParams {
  key_category category;
  std::string name;
  RangeSizeGenerator* range_size_generator;
  double probability;
};

class ProcessingUnit {
  Randomizer& randomizer_;
  // CollectionHub& collections_;
  // mongocxx::collection& db_col_;
  typename DocumentBatch::queue_t& queue_;
  bsoncxx::stdx::string_view name_;
  bsoncxx::document::element model_;
  size_t nb_instances_{0u};
  double weight_{1.};

  size_t bulk_size_{1u};
  std::vector<bsoncxx::document::value> bulk_docs_;
  std::map<std::string, std::pair<mongocxx::cursor, typename mongocxx::cursor::iterator>>
      foreign_cursors_;

  using key_params = std::tuple<key_category, std::string, RangeSizeGenerator*>;
  std::map<bsoncxx::stdx::string_view, KeyParams> key_params_;

  KeyParams& get_key_params(bsoncxx::stdx::string_view key);
  // bsoncxx::document::view get_foreign_view(bsoncxx::stdx::string_view foreign_name);
  void process_element(bsoncxx::array::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::document& ctx);

 public:
  ProcessingUnit(Randomizer& randomizer, typename DocumentBatch::queue_t& queue,
                 bsoncxx::stdx::string_view name, bsoncxx::document::view const& collection,
                 double normalized_weight);

  // Returns idle time
  typename DocumentBatch::queue_t::duration_t process_tick();
  bsoncxx::stdx::string_view name() const;
  size_t nb_inserted() const;
};

}  // namespace mongo_smasher
