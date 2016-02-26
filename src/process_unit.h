#pragma once
#include "randomizer.h"
#include "queue.h"
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
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

class ProcessingUnit {
  Randomizer& randomizer_;
  //CollectionHub& collections_;
  //mongocxx::collection& db_col_;
  Queue<std::vector<bsoncxx::builder::stream::document>>& queue_;
  bsoncxx::stdx::string_view name_;
  bsoncxx::document::element model_;
  size_t nb_instances_{0u};
  double weight_{1.};

  size_t bulk_size_{1u};
  std::vector<bsoncxx::builder::stream::document> bulk_docs_;
  std::vector<bsoncxx::document::view> bulk_views_;
  std::map<std::string, std::pair<mongocxx::cursor, typename mongocxx::cursor::iterator>>
      foreign_cursors_;

  using key_params = std::tuple<key_category, std::string, RangeSizeGenerator*>;
  std::map<bsoncxx::stdx::string_view, key_params> key_params_;

  key_params& get_key_params(bsoncxx::stdx::string_view key);
  // bsoncxx::document::view get_foreign_view(bsoncxx::stdx::string_view foreign_name);
  void process_element(bsoncxx::array::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::document& ctx);

 public:
  using queue_t = Queue<std::vector<bsoncxx::builder::stream::document>>;
  ProcessingUnit(Randomizer& randomizer, queue_t& queue,
                 bsoncxx::stdx::string_view name, bsoncxx::document::view const& collection);
  void process_tick();
  bsoncxx::stdx::string_view name() const;
  size_t nb_inserted() const;
};

}  // namespace mongo_smasher
