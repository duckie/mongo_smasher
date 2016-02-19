#pragma once
#include <vector>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/collection.hpp>
#include "queue.h"

namespace mongo_smasher {

struct ElementToPush {
  bsoncxx::stdx::string_view db;
  bsoncxx::stdx::string_view col;
  std::vector<bsoncxx::builder::stream::document> docs;
};

class CollectionConsumer {

public:
  CollectionConsumer(std::string db_uri);
}

}  // namespace mongo_smasher
