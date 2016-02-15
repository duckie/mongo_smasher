#pragma once
#include "randomizer.h"
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>

namespace mongo_smasher {

struct Collection {
  bsoncxx::stdx::string_view name;
  bsoncxx::document::element model;
  double weight;
};

class ProcessingUnit {
  Randomizer& randomizer_;
  mongocxx::client db_conn_; 
  bsoncxx::stdx::string_view name_;
  bsoncxx::document::element model_;
  size_t nb_instances_ = 0u;
  double weight_ = 1.;

  size_t bulk_size_ = 1u;
  std::vector<bsoncxx::builder::stream::document> bulk_docs_;
  std::vector<bsoncxx::document::view> bulk_views_;

  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::document& ctx);

 public:
  ProcessingUnit(Randomizer& randomizer, std::string const& db_uri, bsoncxx::stdx::string_view name, bsoncxx::document::view const& collection);
  void process_tick();
  bsoncxx::stdx::string_view name() const;
  size_t nb_inserted() const;
};

}  // namespace mongo_smasher