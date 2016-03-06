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
  std::unique_ptr<RangeSizeGenerator> range_size_generator;
  double probability;
};

// Used for ValueParams
// stale: A token to be copied as is
// value: A reference to a value
enum class token_type { stale, value };

struct TokenParams {
  token_type type;
  std::string content;
  std::unique_ptr<ValuePusher> pusher;
};

// stale : A string that do not match any value keys, to be copied as is
// simple: A reference to a single value
// compound: A string concatenating values
enum class value_category { stale, simple, compound };

struct ValueParams {
  value_category category;
  std::vector<TokenParams> values;
};

class InsertUnit {
  Randomizer& randomizer_;
  typename DocumentBatch::queue_t& queue_;
  bsoncxx::stdx::string_view name_;
  bsoncxx::document::element model_;
  size_t nb_instances_{0u};
  double weight_{1.};

  size_t bulk_size_{1u};
  std::vector<bsoncxx::document::value> bulk_docs_;
  std::map<std::string, std::pair<mongocxx::cursor, typename mongocxx::cursor::iterator>>
      foreign_cursors_;

  std::map<bsoncxx::stdx::string_view, KeyParams> key_params_;
  std::map<bsoncxx::stdx::string_view, ValueParams> value_params_;

  KeyParams& get_key_params(bsoncxx::stdx::string_view key);
  ValueParams& get_value_params(bsoncxx::stdx::string_view value);

  // Private member templates can be implemented in compilation unit
  template <class T>
  void process_value(T& ctx, ValueParams& value_params);
  template <class T>
  void process_element(T const& element, bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::array::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element,
                       bsoncxx::builder::stream::document& ctx);

 public:
  InsertUnit(Randomizer& randomizer, typename DocumentBatch::queue_t& queue,
                 bsoncxx::stdx::string_view name, bsoncxx::document::view const& collection,
                 double normalized_weight);

  // 
  // Produces a bulk of documents and return the time wasted on queue
  // 
  // Queues could block if the consumers are too slow, making 
  // InsertUnit to delay.
  //
  typename DocumentBatch::queue_t::duration_t process_tick();

  // Returns total of documents produces since beginning
  size_t nb_inserted() const;
};

}  // namespace mongo_smasher
