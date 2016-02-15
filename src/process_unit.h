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
  mongocxx::client db_conn_; 
  Randomizer& randomizer_;
  std::vector<Collection> collections_;

  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::document& ctx);

 public:
  ProcessingUnit(Randomizer& randomizer, std::string const& db_uri, bsoncxx::document::view collections);
  void process_tick();
};

}  // namespace mongo_smasher
